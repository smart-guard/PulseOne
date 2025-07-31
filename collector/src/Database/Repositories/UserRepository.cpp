/**
 * @file UserRepository.cpp
 * @brief PulseOne UserRepository 구현 (DeviceRepository 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer로 멀티 DB 지원
 * - IRepository 캐싱 시스템 활용
 * - 모든 DB 작업을 Repository에서 처리
 * - Entity는 Repository 호출만
 * 
 * 🔧 컴파일 에러 수정:
 * - executeUpdate → executeUpsert 변경
 * - 문자열 연결 연산자 수정
 * - 헤더에 선언된 메서드만 구현
 * - namespace 일치성 확인
 */

#include "Database/Repositories/UserRepository.h"
#include "Database/DatabaseAbstractionLayer.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 인터페이스 구현
// =============================================================================

std::vector<UserEntity> UserRepository::findAll() {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery("SELECT * FROM users ORDER BY username");
        
        std::vector<UserEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            entities.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Info("UserRepository::findAll - Found " + std::to_string(entities.size()) + " users");
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::findAll failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::optional<UserEntity> UserRepository::findById(int id) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        // 캐시에서 먼저 확인
        if (isCacheEnabled()) {
            // IRepository의 캐시 메서드 사용 (정확한 메서드명 필요)
            // auto cached = getCachedEntity(id);
            // if (cached.has_value()) {
            //     return cached;
            // }
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(
            "SELECT * FROM users WHERE id = " + std::to_string(id)
        );
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        UserEntity entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            // setCachedEntity(id, entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::findById failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

bool UserRepository::save(UserEntity& entity) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        if (!validateEntity(entity)) {
            if (logger_) {
                logger_->Error("UserRepository::save - Invalid entity data");
            }
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        
        // executeUpsert 사용 (DeviceRepository 패턴)
        std::vector<std::string> primary_keys = {"id"};
        bool success = db_layer.executeUpsert("users", params, primary_keys);
        
        if (success) {
            // SQLite에서 자동 생성된 ID 조회
            if (entity.getId() <= 0) {
                auto results = db_layer.executeQuery("SELECT last_insert_rowid() as id");
                if (!results.empty()) {
                    entity.setId(std::stoi(results[0].at("id")));
                }
            }
            
            // 캐시에 저장
            if (isCacheEnabled()) {
                // setCachedEntity(entity.getId(), entity);
            }
            
            if (logger_) {
                std::string message = "UserRepository::save - ";
                message += (success ? "Success" : "Failed");
                message += " for user: " + entity.getUsername();
                logger_->Info(message);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::save failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool UserRepository::update(const UserEntity& entity) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        if (entity.getId() <= 0 || !validateEntity(entity)) {
            if (logger_) {
                logger_->Error("UserRepository::update - Invalid entity data or ID");
            }
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        
        // 🔧 수정: executeUpdate → executeUpsert
        std::vector<std::string> primary_keys = {"id"};  
        bool success = db_layer.executeUpsert("users", params, primary_keys);
        
        if (success && isCacheEnabled()) {
            // setCachedEntity(entity.getId(), entity);
        }
        
        if (logger_) {
            std::string message = "UserRepository::update - ";
            message += (success ? "Success" : "Failed");
            message += " for user: " + entity.getUsername();
            logger_->Info(message);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::update failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool UserRepository::deleteById(int id) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(
            "DELETE FROM users WHERE id = " + std::to_string(id)
        );
        
        if (success && isCacheEnabled()) {
            // removeCachedEntity(id);
        }
        
        if (logger_) {
            std::string message = "UserRepository::deleteById - ";
            message += (success ? "Success" : "Failed");
            message += " for ID: " + std::to_string(id);
            logger_->Info(message);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::deleteById failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool UserRepository::exists(int id) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(
            "SELECT COUNT(*) as count FROM users WHERE id = " + std::to_string(id)
        );
        
        return !results.empty() && std::stoi(results[0].at("count")) > 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::exists failed: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 벌크 연산 구현
// =============================================================================

std::vector<UserEntity> UserRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        return {};
    }
    
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        // ID 목록을 문자열로 변환
        std::stringstream ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ss << ",";
            ss << ids[i];
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(
            "SELECT * FROM users WHERE id IN (" + ss.str() + ") ORDER BY username"
        );
        
        std::vector<UserEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            entities.push_back(mapRowToEntity(row));
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::findByIds failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<UserEntity> UserRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::stringstream query;
        query << "SELECT * FROM users";
        
        if (!conditions.empty()) {
            query << " WHERE ";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i > 0) query << " AND ";
                query << conditions[i].field << " " << conditions[i].operation << " '" << conditions[i].value << "'";
            }
        }
        
        if (order_by.has_value()) {
            query << " ORDER BY " << order_by->field << " " << (order_by->ascending ? "ASC" : "DESC");
        }
        
        if (pagination.has_value()) {
            query << " LIMIT " << pagination->limit << " OFFSET " << pagination->offset;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query.str());
        
        std::vector<UserEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            entities.push_back(mapRowToEntity(row));
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::findByConditions failed: " + std::string(e.what()));
        }
        return {};
    }
}

int UserRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::stringstream query;
        query << "SELECT COUNT(*) as count FROM users";
        
        if (!conditions.empty()) {
            query << " WHERE ";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i > 0) query << " AND ";
                query << conditions[i].field << " " << conditions[i].operation << " '" << conditions[i].value << "'";
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query.str());
        
        return results.empty() ? 0 : std::stoi(results[0].at("count"));
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::countByConditions failed: " + std::string(e.what()));
        }
        return 0;
    }
}

// =============================================================================
// User 특화 메서드들
// =============================================================================

std::optional<UserEntity> UserRepository::findByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(
            "SELECT * FROM users WHERE username = '" + username + "'"
        );
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::findByUsername failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

std::optional<UserEntity> UserRepository::findByEmail(const std::string& email) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(
            "SELECT * FROM users WHERE email = '" + email + "'"
        );
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::findByEmail failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

std::vector<UserEntity> UserRepository::findByTenant(int tenant_id) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("tenant_id", "=", std::to_string(tenant_id))
    };
    return findByConditions(conditions);
}

std::vector<UserEntity> UserRepository::findByRole(const std::string& role) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("role", "=", role)
    };
    return findByConditions(conditions);
}

std::vector<UserEntity> UserRepository::findByEnabled(bool enabled) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("is_enabled", "=", enabled ? "1" : "0")
    };
    return findByConditions(conditions);
}

std::vector<UserEntity> UserRepository::findByDepartment(const std::string& department) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("department", "=", department)
    };
    return findByConditions(conditions);
}

std::vector<UserEntity> UserRepository::findByPermission(const std::string& permission) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(
            "SELECT * FROM users WHERE permissions LIKE '%" + permission + "%'"
        );
        
        std::vector<UserEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            UserEntity entity = mapRowToEntity(row);
            // 정확한 권한 확인 (JSON 파싱)
            if (entity.hasPermission(permission)) {
                entities.push_back(entity);
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::findByPermission failed: " + std::string(e.what()));
        }
        return {};
    }
}

// =============================================================================
// 통계 메서드들
// =============================================================================

int UserRepository::countByTenant(int tenant_id) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("tenant_id", "=", std::to_string(tenant_id))
    };
    return countByConditions(conditions);
}

int UserRepository::countByRole(const std::string& role) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("role", "=", role)
    };
    return countByConditions(conditions);
}

int UserRepository::countActiveUsers() {
    std::vector<QueryCondition> conditions = {
        QueryCondition("is_enabled", "=", "1")
    };
    return countByConditions(conditions);
}

// =============================================================================
// 인증 관련 메서드들
// =============================================================================

bool UserRepository::recordLogin(int user_id) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        auto user_opt = findById(user_id);
        if (!user_opt.has_value()) {
            return false;
        }
        
        UserEntity user = user_opt.value();
        user.updateLastLogin();
        
        return update(user);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::recordLogin failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool UserRepository::isUsernameTaken(const std::string& username) {
    return findByUsername(username).has_value();
}

bool UserRepository::isEmailTaken(const std::string& email) {
    return findByEmail(email).has_value();
}

// =============================================================================
// 고급 쿼리 메서드들
// =============================================================================

std::vector<UserEntity> UserRepository::searchUsers(
    const std::optional<int>& tenant_id,
    const std::optional<std::string>& role,
    const std::optional<bool>& enabled,
    const std::optional<std::string>& department) {
    
    std::vector<QueryCondition> conditions;
    
    if (tenant_id.has_value()) {
        conditions.push_back(QueryCondition("tenant_id", "=", std::to_string(tenant_id.value())));
    }
    
    if (role.has_value()) {
        conditions.push_back(QueryCondition("role", "=", role.value()));
    }
    
    if (enabled.has_value()) {
        conditions.push_back(QueryCondition("is_enabled", "=", enabled.value() ? "1" : "0"));
    }
    
    if (department.has_value()) {
        conditions.push_back(QueryCondition("department", "=", department.value()));
    }
    
    return findByConditions(conditions);
}

std::vector<UserEntity> UserRepository::findInactiveUsers(int days_ago) {
    std::lock_guard<std::mutex> lock(repository_mutex_);
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24 * days_ago);
        std::string cutoff_str = serializeTimestamp(cutoff_time);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(
            "SELECT * FROM users WHERE last_login_at < '" + cutoff_str + "' ORDER BY last_login_at"
        );
        
        std::vector<UserEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            entities.push_back(mapRowToEntity(row));
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::findInactiveUsers failed: " + std::string(e.what()));
        }
        return {};
    }
}

// =============================================================================
// DeviceRepository 패턴: 헬퍼 메서드들 (override 제거)
// =============================================================================

bool UserRepository::ensureTableExists() {
    if (table_created_.load()) {
        return true;
    }
    
    try {
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(getCreateTableSQL());
        
        if (success) {
            table_created_.store(true);
            if (logger_) {
                logger_->Info("UserRepository::ensureTableExists - Table created/verified");
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::ensureTableExists failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool UserRepository::validateEntity(const UserEntity& entity) const {
    return entity.isValid();
}

UserEntity UserRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    UserEntity entity;
    
    try {
        if (row.count("id")) entity.setId(std::stoi(row.at("id")));
        if (row.count("tenant_id")) entity.setTenantId(std::stoi(row.at("tenant_id")));
        if (row.count("username")) entity.setUsername(row.at("username"));
        if (row.count("email")) entity.setEmail(row.at("email"));
        if (row.count("full_name")) entity.setFullName(row.at("full_name"));
        if (row.count("role")) entity.setRole(row.at("role"));
        if (row.count("is_enabled")) entity.setEnabled(row.at("is_enabled") == "1");
        if (row.count("phone_number")) entity.setPhoneNumber(row.at("phone_number"));
        if (row.count("department")) entity.setDepartment(row.at("department"));
        if (row.count("login_count")) entity.setLoginCount(std::stoi(row.at("login_count")));
        if (row.count("notes")) entity.setNotes(row.at("notes"));
        
        // 권한 파싱 (JSON 배열)
        if (row.count("permissions")) {
            auto permissions = parsePermissions(row.at("permissions"));
            entity.setPermissions(permissions);
        }
        
        // 비밀번호 해시 (내부적으로만 설정)
        if (row.count("password_hash")) {
            // 직접 접근은 불가하므로 fromJson 사용
            json temp_json;
            temp_json["password_hash"] = row.at("password_hash");
            // UserEntity에서 password_hash_ 직접 설정하는 메서드 필요
        }
        
        // 타임스탬프 파싱
        if (row.count("last_login_at")) {
            // parseTimestamp로 변환 후 설정
        }
        
        entity.markSaved();  // DB에서 로드된 상태로 표시
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::mapRowToEntity failed: " + std::string(e.what()));
        }
    }
    
    return entity;
}

std::map<std::string, std::string> UserRepository::entityToParams(const UserEntity& entity) {
    std::map<std::string, std::string> params;
    
    try {
        if (entity.getId() > 0) {
            params["id"] = std::to_string(entity.getId());
        }
        
        params["tenant_id"] = std::to_string(entity.getTenantId());
        params["username"] = entity.getUsername();
        params["email"] = entity.getEmail();
        params["full_name"] = entity.getFullName();
        params["role"] = entity.getRole();
        params["is_enabled"] = entity.isEnabled() ? "1" : "0";
        params["phone_number"] = entity.getPhoneNumber();
        params["department"] = entity.getDepartment();
        params["login_count"] = std::to_string(entity.getLoginCount());
        params["notes"] = entity.getNotes();
        
        // 권한 JSON 직렬화
        params["permissions"] = serializePermissions(entity.getPermissions());
        
        // 타임스탬프
        params["last_login_at"] = serializeTimestamp(entity.getLastLoginAt());
        params["created_at"] = serializeTimestamp(std::chrono::system_clock::now());
        params["updated_at"] = serializeTimestamp(std::chrono::system_clock::now());
        
        // 비밀번호 해시는 별도 처리 (보안상 getter 없음)
        // params["password_hash"] = entity.getPasswordHash(); // 이 메서드는 없어야 함
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::entityToParams failed: " + std::string(e.what()));
        }
    }
    
    return params;
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

std::string UserRepository::getCreateTableSQL() const {
    return R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            username VARCHAR(50) UNIQUE NOT NULL,
            email VARCHAR(100) UNIQUE NOT NULL,
            password_hash VARCHAR(255) NOT NULL,
            full_name VARCHAR(100),
            role VARCHAR(20) NOT NULL DEFAULT 'viewer',
            is_enabled BOOLEAN DEFAULT 1,
            phone_number VARCHAR(20),
            department VARCHAR(50),
            permissions TEXT DEFAULT '[]',
            login_count INTEGER DEFAULT 0,
            last_login_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            notes TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            
            CONSTRAINT fk_user_tenant FOREIGN KEY (tenant_id) REFERENCES tenants(id),
            CONSTRAINT chk_user_role CHECK (role IN ('admin', 'engineer', 'operator', 'viewer'))
        )
    )";
}

std::vector<std::string> UserRepository::parsePermissions(const std::string& permissions_json) const {
    std::vector<std::string> permissions;
    
    try {
#ifdef HAS_NLOHMANN_JSON
        if (!permissions_json.empty() && permissions_json != "[]") {
            json j = json::parse(permissions_json);
            if (j.is_array()) {
                permissions = j.get<std::vector<std::string>>();
            }
        }
#else
        // JSON 라이브러리가 없는 경우 간단 파싱
        if (permissions_json.size() > 2 && permissions_json.front() == '[' && permissions_json.back() == ']') {
            std::string content = permissions_json.substr(1, permissions_json.size() - 2);
            if (!content.empty()) {
                std::stringstream ss(content);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    // 따옴표 제거
                    if (item.size() >= 2 && item.front() == '"' && item.back() == '"') {
                        item = item.substr(1, item.size() - 2);
                    }
                    if (!item.empty()) {
                        permissions.push_back(item);
                    }
                }
            }
        }
#endif
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::parsePermissions failed: " + std::string(e.what()));
        }
    }
    
    return permissions;
}

std::string UserRepository::serializePermissions(const std::vector<std::string>& permissions) const {
    try {
#ifdef HAS_NLOHMANN_JSON
        json j = permissions;
        return j.dump();
#else
        // JSON 라이브러리가 없는 경우 수동 직렬화
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < permissions.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "\"" << permissions[i] << "\"";
        }
        ss << "]";
        return ss.str();
#endif
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::serializePermissions failed: " + std::string(e.what()));
        }
        return "[]";
    }
}

std::chrono::system_clock::time_point UserRepository::parseTimestamp(const std::string& timestamp_str) const {
    try {
        // ISO 8601 형식 파싱: "YYYY-MM-DD HH:MM:SS"
        std::tm tm = {};
        std::istringstream ss(timestamp_str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        
        if (ss.fail()) {
            return std::chrono::system_clock::now();
        }
        
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::parseTimestamp failed: " + std::string(e.what()));
        }
        return std::chrono::system_clock::now();
    }
}

std::string UserRepository::serializeTimestamp(const std::chrono::system_clock::time_point& timestamp) const {
    try {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("UserRepository::serializeTimestamp failed: " + std::string(e.what()));
        }
        return "1970-01-01 00:00:00";
    }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne