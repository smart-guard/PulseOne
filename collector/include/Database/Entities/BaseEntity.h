#ifndef PULSEONE_BASE_ENTITY_H
#define PULSEONE_BASE_ENTITY_H

/**
 * @file BaseEntity.h
 * @brief PulseOne 기본 엔티티 템플릿 - 간단한 Repository 패턴
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 간단한 Repository 패턴:
 * - 각 Entity가 자기 Repository를 알아서 호출
 * - 중복 코드 제거
 * - Forward Declaration 불필요
 */


#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Common/BasicTypes.h"           // UUID, Timestamp 등
#include "Common/Enums.h"                // ProtocolType, ConnectionStatus 등  
#include "Common/Structs.h"              // DeviceInfo, DataPoint 등
#include "Common/DriverStatistics.h"     // DriverStatistics
#include <string>
#include <optional>
#include <chrono>
#include <map>
#include <sstream>
#include <iomanip>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#else
using json = nlohmann::json;
#endif

namespace PulseOne {
namespace Database {

    namespace Repositories {
    class DeviceRepository;
    class DeviceSettingsRepository;
    class DataPointRepository;
    class CurrentValueRepository;
    class VirtualPointRepository;
    class SiteRepository;
    class TenantRepository;
    class UserRepository;
    class AlarmConfigRepository;
}
/**
 * @brief 엔티티 상태 열거형
 */
enum class EntityState {
    NEW,           // 새로 생성됨 (DB에 없음)
    LOADED,        // DB에서 로드됨
    MODIFIED,      // 수정됨 (DB 업데이트 필요)
    DELETED,       // 삭제 예정
    ERROR          // 에러 상태
};

/**
 * @brief 모든 엔티티의 기본 클래스 (템플릿)
 * @tparam DerivedType 파생 클래스 타입 (CRTP 패턴)
 */
template<typename DerivedType>
class BaseEntity {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자
     */
    BaseEntity() 
        : id_(0)
        , state_(EntityState::NEW)
        , created_at_(std::chrono::system_clock::now())
        , updated_at_(std::chrono::system_clock::now())
        , db_manager_(&DatabaseManager::getInstance())
        , config_manager_(&ConfigManager::getInstance())
        , logger_(&LogManager::getInstance()) {}
    
    /**
     * @brief ID로 생성하는 생성자
     * @param id 엔티티 ID
     */
    explicit BaseEntity(int id) : BaseEntity() {
        id_ = id;
        if (id > 0) {
            state_ = EntityState::LOADED;
        }
    }
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~BaseEntity() = default;

    // =======================================================================
    // 🔥 복사/이동 생성자 및 할당 연산자 - 명시적 선언 필요
    // =======================================================================
    
    /**
     * @brief 복사 생성자 - 명시적 선언 (이동 할당 연산자 때문에 삭제되는 것 방지)
     */
    BaseEntity(const BaseEntity& other) 
        : id_(other.id_)
        , state_(other.state_)
        , created_at_(other.created_at_)
        , updated_at_(other.updated_at_)
        , db_manager_(&DatabaseManager::getInstance())      // 동일한 싱글톤 참조
        , config_manager_(&ConfigManager::getInstance())    // 동일한 싱글톤 참조
        , logger_(&LogManager::getInstance()) {}  // 동일한 싱글톤 참조
    
    /**
     * @brief 이동 생성자
     */
    BaseEntity(BaseEntity&& other) noexcept
        : id_(other.id_)
        , state_(other.state_)
        , created_at_(other.created_at_)
        , updated_at_(other.updated_at_)
        , db_manager_(&DatabaseManager::getInstance())      // 동일한 싱글톤 참조
        , config_manager_(&ConfigManager::getInstance())    // 동일한 싱글톤 참조
        , logger_(&LogManager::getInstance()) {}  // 동일한 싱글톤 참조

    // =======================================================================
    // 🔥 할당 연산자들 (기존에 있던 것들)
    // =======================================================================
    
    /**
     * @brief 복사 할당 연산자
     */
    BaseEntity& operator=(const BaseEntity& other) {
        if (this != &other) {
            id_ = other.id_;
            state_ = other.state_;
            created_at_ = other.created_at_;
            updated_at_ = other.updated_at_;
            // 포인터들은 동일한 싱글톤을 가리키므로 복사할 필요 없음
        }
        return *this;
    }
    
    /**
     * @brief 이동 할당 연산자
     */
    BaseEntity& operator=(BaseEntity&& other) noexcept {
        if (this != &other) {
            id_ = other.id_;
            state_ = other.state_;
            created_at_ = other.created_at_;
            updated_at_ = other.updated_at_;
            // 포인터들은 동일한 싱글톤을 가리키므로 이동할 필요 없음
        }
        return *this;
    }

    // =======================================================================
    // 🎯 NEW: Repository 자동 선택 패턴
    // =======================================================================

protected:
    /**
     * @brief 타입별 Repository 자동 선택 - CRTP 활용
     * @return 해당 Entity 타입의 Repository
     */
    template<typename EntityType = DerivedType>
    auto getRepository() -> decltype(getRepositoryImpl(static_cast<EntityType*>(nullptr))) {
        return getRepositoryImpl(static_cast<EntityType*>(nullptr));
    }

    /**
     * @brief Repository를 통한 자동 저장
     * @return 성공 시 true
     */
    bool saveViaRepository() {
        try {
            auto repo = getRepository();
            if (repo) {
                DerivedType& derived = static_cast<DerivedType&>(*this);
                bool success = repo->save(derived);
                if (success) {
                    markSaved();
                    logger_->Info("BaseEntity::saveViaRepository - Saved " + getEntityTypeName());
                }
                return success;
            }
            return false;
        } catch (const std::exception& e) {
            logger_->Error("BaseEntity::saveViaRepository failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }

    /**
     * @brief Repository를 통한 자동 업데이트
     * @return 성공 시 true
     */
    bool updateViaRepository() {
        try {
            auto repo = getRepository();
            if (repo) {
                const DerivedType& derived = static_cast<const DerivedType&>(*this);
                bool success = repo->update(derived);
                if (success) {
                    markSaved();
                    logger_->Info("BaseEntity::updateViaRepository - Updated " + getEntityTypeName());
                }
                return success;
            }
            return false;
        } catch (const std::exception& e) {
            logger_->Error("BaseEntity::updateViaRepository failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }

    /**
     * @brief Repository를 통한 자동 삭제
     * @return 성공 시 true
     */
    bool deleteViaRepository() {
        try {
            auto repo = getRepository();
            if (repo) {
                bool success = repo->deleteById(getEntityId());
                if (success) {
                    markDeleted();
                    logger_->Info("BaseEntity::deleteViaRepository - Deleted " + getEntityTypeName());
                }
                return success;
            }
            return false;
        } catch (const std::exception& e) {
            logger_->Error("BaseEntity::deleteViaRepository failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }

    /**
     * @brief Repository를 통한 자동 로드
     * @return 성공 시 true
     */
    bool loadViaRepository() {
        try {
            auto repo = getRepository();
            if (repo) {
                auto loaded = repo->findById(getEntityId());
                if (loaded.has_value()) {
                    DerivedType& derived = static_cast<DerivedType&>(*this);
                    derived = loaded.value();
                    markSaved();
                    logger_->Info("BaseEntity::loadViaRepository - Loaded " + getEntityTypeName());
                    return true;
                }
            }
            return false;
        } catch (const std::exception& e) {
            logger_->Error("BaseEntity::loadViaRepository failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }

private:
    // =======================================================================
    // Repository 타입별 특화 (SFINAE 패턴)
    // =======================================================================

    // DeviceEntity 특화
    std::shared_ptr<Repositories::DeviceRepository> getRepositoryImpl(class DeviceEntity*);

    // DeviceSettingsEntity 특화
    std::shared_ptr<Repositories::DeviceSettingsRepository> getRepositoryImpl(class DeviceSettingsEntity*);

    // DataPointEntity 특화
    std::shared_ptr<Repositories::DataPointRepository> getRepositoryImpl(class DataPointEntity*);

    // CurrentValueEntity 특화
    std::shared_ptr<Repositories::CurrentValueRepository> getRepositoryImpl(class CurrentValueEntity*);

    // 다른 Entity들...
    std::shared_ptr<Repositories::VirtualPointRepository> getRepositoryImpl(class VirtualPointEntity*);
    std::shared_ptr<Repositories::SiteRepository> getRepositoryImpl(class SiteEntity*);
    std::shared_ptr<Repositories::TenantRepository> getRepositoryImpl(class TenantEntity*);
    std::shared_ptr<Repositories::UserRepository> getRepositoryImpl(class UserEntity*);
    std::shared_ptr<Repositories::AlarmConfigRepository> getRepositoryImpl(class AlarmConfigEntity*);

    /**
     * @brief Entity ID 조회 (타입별 다를 수 있음)
     */
    int getEntityId() const {
        // 대부분의 Entity는 id_ 사용
        if constexpr (std::is_same_v<DerivedType, class DeviceSettingsEntity>) {
            // DeviceSettingsEntity는 device_id_ 사용
            return static_cast<const DerivedType&>(*this).getDeviceId();
        } else {
            return id_;
        }
    }

    /**
     * @brief Entity 타입명 조회 (디버깅용)
     */
    std::string getEntityTypeName() const {
        if constexpr (std::is_same_v<DerivedType, class DeviceEntity>) {
            return "DeviceEntity";
        } else if constexpr (std::is_same_v<DerivedType, class DeviceSettingsEntity>) {
            return "DeviceSettingsEntity";
        } else if constexpr (std::is_same_v<DerivedType, class DataPointEntity>) {
            return "DataPointEntity";
        } else if constexpr (std::is_same_v<DerivedType, class CurrentValueEntity>) {
            return "CurrentValueEntity";
        } else {
            return "UnknownEntity";
        }
    }

public:
    // =======================================================================
    // 공통 DB 연산 (순수 가상 함수) - 기존과 동일
    // =======================================================================
    
    /**
     * @brief 데이터베이스에서 로드 (파생 클래스에서 구현)
     * @return 성공 시 true
     */
    virtual bool loadFromDatabase() = 0;
    
    /**
     * @brief 데이터베이스에 저장 (파생 클래스에서 구현)
     * @return 성공 시 true
     */
    virtual bool saveToDatabase() = 0;
    
    /**
     * @brief 데이터베이스에 업데이트 (파생 클래스에서 구현)
     * @return 성공 시 true
     */
    virtual bool updateToDatabase() = 0;
    
    /**
     * @brief 데이터베이스에서 삭제 (파생 클래스에서 구현)
     * @return 성공 시 true
     */
    virtual bool deleteFromDatabase() = 0;

    // =======================================================================
    // 공통 JSON 직렬화 (순수 가상 함수) - 기존과 동일
    // =======================================================================
    
    /**
     * @brief JSON으로 변환 (파생 클래스에서 구현)
     * @return JSON 객체
     */
    virtual json toJson() const = 0;
    
    /**
     * @brief JSON에서 로드 (파생 클래스에서 구현)
     * @param data JSON 데이터
     * @return 성공 시 true
     */
    virtual bool fromJson(const json& data) = 0;
    
    /**
     * @brief 문자열 표현 (파생 클래스에서 구현)
     * @return 엔티티 정보 문자열
     */
    virtual std::string toString() const = 0;

    // =======================================================================
    // 공통 접근자 (getter/setter) - 기존과 동일
    // =======================================================================
    
    /**
     * @brief 엔티티 ID 조회
     */
    int getId() const { return id_; }
    
    /**
     * @brief 엔티티 ID 설정
     * @param id 엔티티 ID
     */
    void setId(int id) { 
        id_ = id; 
        markModified();
    }
    
    /**
     * @brief 엔티티 상태 조회
     */
    EntityState getState() const { return state_; }
    
    /**
     * @brief 생성 시간 조회
     */
    std::chrono::system_clock::time_point getCreatedAt() const { return created_at_; }
    
    /**
     * @brief 수정 시간 조회
     */
    std::chrono::system_clock::time_point getUpdatedAt() const { return updated_at_; }

    // =======================================================================
    // 공통 상태 관리 - 기존과 동일
    // =======================================================================
    
    /**
     * @brief DB에서 로드된 상태인지 확인
     */
    bool isLoadedFromDb() const { return state_ == EntityState::LOADED; }
    
    /**
     * @brief 새로 생성된 상태인지 확인
     */
    bool isNew() const { return state_ == EntityState::NEW; }
    
    /**
     * @brief 수정된 상태인지 확인
     */
    bool isModified() const { return state_ == EntityState::MODIFIED; }
    
    /**
     * @brief 삭제 예정 상태인지 확인
     */
    bool isDeleted() const { return state_ == EntityState::DELETED; }
    
    /**
     * @brief 에러 상태인지 확인
     */
    bool isError() const { return state_ == EntityState::ERROR; }
    
    /**
     * @brief 유효한 상태인지 확인
     */
    virtual bool isValid() const {
        return state_ != EntityState::ERROR && id_ >= 0;
    }

    // =======================================================================
    // 공통 유틸리티 - 기존과 동일
    // =======================================================================
    
    /**
     * @brief 수정됨으로 상태 변경
     */
    void markModified() {
        if (state_ == EntityState::LOADED) {
            state_ = EntityState::MODIFIED;
            updated_at_ = std::chrono::system_clock::now();
        }
    }
    
    /**
     * @brief 삭제 예정으로 상태 변경
     */
    void markDeleted() {
        state_ = EntityState::DELETED;
        updated_at_ = std::chrono::system_clock::now();
    }
    
    /**
     * @brief 에러 상태로 변경
     */
    void markError() {
        state_ = EntityState::ERROR;
        updated_at_ = std::chrono::system_clock::now();
    }
    
    /**
     * @brief 저장 완료 후 상태 업데이트
     */
    void markSaved() {
        state_ = EntityState::LOADED;
        updated_at_ = std::chrono::system_clock::now();
    }

protected:
    // =======================================================================
    // 보호된 헬퍼 메서드들 - 기존과 동일
    // =======================================================================
    
    /**
     * @brief 통합 쿼리 실행 (DB 타입 자동 처리)
     * @param sql SQL 쿼리
     * @return 결과 맵의 벡터
     */
    std::vector<std::map<std::string, std::string>> executeUnifiedQuery(const std::string& sql) {
        try {
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            
            if (db_type == "POSTGRESQL") {
                return executePostgresQuery(sql);
            } else {
                return executeSQLiteQuery(sql);
            }
        } catch (const std::exception& e) {
            logger_->Error("executeUnifiedQuery failed: " + std::string(e.what()));
            markError();
            return {};
        }
    }
    
    /**
     * @brief 통합 비쿼리 실행 (INSERT/UPDATE/DELETE)
     * @param sql SQL 쿼리
     * @return 성공 시 true
     */
    bool executeUnifiedNonQuery(const std::string& sql) {
        try {
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            
            if (db_type == "POSTGRESQL") {
                return db_manager_->executeNonQueryPostgres(sql);
            } else {
                return db_manager_->executeNonQuerySQLite(sql);
            }
        } catch (const std::exception& e) {
            logger_->Error("executeUnifiedNonQuery failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }
    
    /**
     * @brief 엔티티 테이블명 조회 (파생 클래스에서 오버라이드)
     */
    virtual std::string getTableName() const = 0;
    
    /**
     * @brief SQL 문자열 이스케이프
     * @param str 원본 문자열
     * @return 이스케이프된 문자열
     */
    std::string escapeString(const std::string& str) const {
        std::string escaped = str;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos) {
            escaped.replace(pos, 1, "''");
            pos += 2;
        }
        return escaped;
    }
    
    /**
     * @brief 타임스탬프를 문자열로 변환
     * @param timestamp 타임스탬프
     * @return ISO 8601 형식 문자열
     */
    std::string timestampToString(const std::chrono::system_clock::time_point& timestamp) const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

private:
    // =======================================================================
    // PostgreSQL 전용 쿼리 실행 - 기존과 동일
    // =======================================================================
    std::vector<std::map<std::string, std::string>> executePostgresQuery(const std::string& sql) {
        // PostgreSQL 구현 (기존 코드 활용)
        std::vector<std::map<std::string, std::string>> results;
        try {
            auto result = db_manager_->executeQueryPostgres(sql);
            for (const auto& row : result) {
                std::map<std::string, std::string> row_map;
                for (size_t i = 0; i < row.size(); ++i) {
                    std::string column_name = result.column_name(i);
                    std::string value = row[static_cast<int>(i)].is_null() ? "" : row[static_cast<int>(i)].as<std::string>();
                    row_map[column_name] = value;
                }
                results.push_back(row_map);
            }
        } catch (const std::exception& e) {
            logger_->Error("PostgreSQL query failed: " + std::string(e.what()));
        }
        return results;
    }
    
    // =======================================================================
    // SQLite 전용 쿼리 실행 - 기존과 동일
    // =======================================================================
    std::vector<std::map<std::string, std::string>> executeSQLiteQuery(const std::string& sql) {
        // SQLite 구현 (콜백 기반)
        std::vector<std::map<std::string, std::string>> results;
        
        auto callback = [](void* data, int argc, char** argv, char** col_names) -> int {
            auto* results_ptr = static_cast<std::vector<std::map<std::string, std::string>>*>(data);
            std::map<std::string, std::string> row;
            
            for (int i = 0; i < argc; i++) {
                std::string value = argv[i] ? argv[i] : "";
                row[col_names[i]] = value;
            }
            
            results_ptr->push_back(row);
            return 0;
        };
        
        bool success = db_manager_->executeQuerySQLite(sql, callback, &results);
        if (!success) {
            logger_->Error("SQLite query failed: " + sql);
            results.clear();
        }
        
        return results;
    }

protected:
    // =======================================================================
    // 멤버 변수들 - 기존과 동일
    // =======================================================================
    
    int id_;                                              // 엔티티 ID
    EntityState state_;                                   // 엔티티 상태
    std::chrono::system_clock::time_point created_at_;    // 생성 시간
    std::chrono::system_clock::time_point updated_at_;    // 수정 시간
    
    // 의존성 (참조로 저장)
    DatabaseManager* db_manager_;
    ConfigManager* config_manager_;
    LogManager* logger_;
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_BASE_ENTITY_H