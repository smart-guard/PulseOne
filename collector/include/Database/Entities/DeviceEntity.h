#ifndef DEVICE_ENTITY_H
#define DEVICE_ENTITY_H

/**
 * @file DeviceEntity.h
 * @brief PulseOne 디바이스 엔티티 (BaseEntity 상속, 헤더 전용, 스키마 완전 매핑)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 스키마 완전 매핑 + 헤더 전용:
 * - BaseEntity<DeviceEntity> 상속 (CRTP)
 * - devices 테이블 스키마와 1:1 완전 매핑
 * - 모든 구현을 헤더에 인라인으로 포함
 * - Repository 패턴으로 DB 작업 위임
 */

#include "Database/Entities/BaseEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceRepository.h"
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace PulseOne {

// Forward declarations
namespace Database {
namespace Repositories {
    class DeviceRepository;
    class RepositoryFactory;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 디바이스 엔티티 클래스 (BaseEntity 상속, 스키마 완전 매핑)
 * 
 * 🎯 DB 스키마 완전 매핑:
 * CREATE TABLE devices (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     tenant_id INTEGER NOT NULL,
 *     site_id INTEGER NOT NULL,
 *     device_group_id INTEGER,
 *     edge_server_id INTEGER,
 *     
 *     -- 디바이스 기본 정보
 *     name VARCHAR(100) NOT NULL,
 *     description TEXT,
 *     device_type VARCHAR(50) NOT NULL,
 *     manufacturer VARCHAR(100),
 *     model VARCHAR(100),
 *     serial_number VARCHAR(100),
 *     
 *     -- 통신 설정
 *     protocol_type VARCHAR(50) NOT NULL,
 *     endpoint VARCHAR(255) NOT NULL,
 *     config TEXT NOT NULL,
 *     
 *     -- 수집 설정
 *     polling_interval INTEGER DEFAULT 1000,
 *     timeout INTEGER DEFAULT 3000,
 *     retry_count INTEGER DEFAULT 3,
 *     
 *     -- 상태 정보
 *     is_enabled INTEGER DEFAULT 1,
 *     installation_date DATE,
 *     last_maintenance DATE,
 *     
 *     created_by INTEGER,
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
 * );
 */
class DeviceEntity : public BaseEntity<DeviceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자  
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (기본값으로 초기화)
     */
    DeviceEntity() 
        : BaseEntity<DeviceEntity>()
        , tenant_id_(0)
        , site_id_(0)
        , device_group_id_(std::nullopt)
        , edge_server_id_(std::nullopt)
        , name_("")
        , description_("")
        , device_type_("")
        , manufacturer_("")
        , model_("")
        , serial_number_("")
        , protocol_type_("")
        , endpoint_("")
        , config_("{}")
        , polling_interval_(1000)
        , timeout_(3000)
        , retry_count_(3)
        , is_enabled_(true)
        , installation_date_(std::nullopt)
        , last_maintenance_(std::nullopt)
        , created_by_(std::nullopt) {
    }
    
    /**
     * @brief ID로 생성하는 생성자
     * @param device_id 디바이스 ID
     */
    explicit DeviceEntity(int device_id) 
        : BaseEntity<DeviceEntity>(device_id)
        , tenant_id_(0)
        , site_id_(0)
        , device_group_id_(std::nullopt)
        , edge_server_id_(std::nullopt)
        , name_("")
        , description_("")
        , device_type_("")
        , manufacturer_("")
        , model_("")
        , serial_number_("")
        , protocol_type_("")
        , endpoint_("")
        , config_("{}")
        , polling_interval_(1000)
        , timeout_(3000)
        , retry_count_(3)
        , is_enabled_(true)
        , installation_date_(std::nullopt)
        , last_maintenance_(std::nullopt)
        , created_by_(std::nullopt) {
    }
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~DeviceEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (Repository에 위임)
    // =======================================================================
    
    /**
     * @brief DB에서 엔티티 로드 (Repository에 위임)
     * @return 성공 시 true
     */
    bool loadFromDatabase() override {
        if (id_ <= 0) {
            logger_->Error("DeviceEntity::loadFromDatabase - Invalid device ID: " + std::to_string(id_));
            markError();
            return false;
        }
        
        try {
            auto repo = getRepository();
            if (repo) {
                auto loaded = repo->findById(id_);
                if (loaded.has_value()) {
                    *this = loaded.value();
                    markSaved();
                    logger_->Info("DeviceEntity::loadFromDatabase - Loaded device: " + name_);
                    return true;
                }
            }
            
            logger_->Warn("DeviceEntity::loadFromDatabase - Device not found: " + std::to_string(id_));
            return false;
            
        } catch (const std::exception& e) {
            logger_->Error("DeviceEntity::loadFromDatabase failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }
    
    /**
     * @brief DB에 엔티티 저장 (Repository에 위임)
     * @return 성공 시 true
     */
    bool saveToDatabase() override {
        if (!isValid()) {
            logger_->Error("DeviceEntity::saveToDatabase - Invalid device data");
            return false;
        }
        
        try {
            auto repo = getRepository();
            if (repo) {
                bool success = repo->save(*this);
                if (success) {
                    markSaved();
                    logger_->Info("DeviceEntity::saveToDatabase - Saved device: " + name_);
                }
                return success;
            }
            return false;
            
        } catch (const std::exception& e) {
            logger_->Error("DeviceEntity::saveToDatabase failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }
    
    /**
     * @brief DB에서 엔티티 삭제 (Repository에 위임)
     * @return 성공 시 true
     */
    bool deleteFromDatabase() override {
        if (id_ <= 0) {
            logger_->Error("DeviceEntity::deleteFromDatabase - Invalid device ID");
            return false;
        }
        
        try {
            auto repo = getRepository();
            if (repo) {
                bool success = repo->deleteById(id_);
                if (success) {
                    markDeleted();
                    logger_->Info("DeviceEntity::deleteFromDatabase - Deleted device ID: " + std::to_string(id_));
                }
                return success;
            }
            return false;
            
        } catch (const std::exception& e) {
            logger_->Error("DeviceEntity::deleteFromDatabase failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }
    
    /**
     * @brief DB에 엔티티 업데이트 (Repository에 위임)
     * @return 성공 시 true
     */
    bool updateToDatabase() override {
        if (id_ <= 0 || !isValid()) {
            logger_->Error("DeviceEntity::updateToDatabase - Invalid device data or ID");
            return false;
        }
        
        try {
            auto repo = getRepository();
            if (repo) {
                bool success = repo->update(*this);
                if (success) {
                    markSaved();
                    logger_->Info("DeviceEntity::updateToDatabase - Updated device: " + name_);
                }
                return success;
            }
            return false;
            
        } catch (const std::exception& e) {
            logger_->Error("DeviceEntity::updateToDatabase failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }
    
    /**
     * @brief JSON으로 직렬화
     * @return JSON 객체
     */
    json toJson() const override {
        json j;
        
        try {
            // 기본 식별자
            j["id"] = id_;
            j["tenant_id"] = tenant_id_;
            j["site_id"] = site_id_;
            
            if (device_group_id_.has_value()) {
                j["device_group_id"] = device_group_id_.value();
            }
            if (edge_server_id_.has_value()) {
                j["edge_server_id"] = edge_server_id_.value();
            }
            
            // 디바이스 기본 정보
            j["name"] = name_;
            j["description"] = description_;
            j["device_type"] = device_type_;
            j["manufacturer"] = manufacturer_;
            j["model"] = model_;
            j["serial_number"] = serial_number_;
            
            // 통신 설정
            j["protocol_type"] = protocol_type_;
            j["endpoint"] = endpoint_;
            
            // config를 JSON으로 파싱해서 포함
            try {
                if (!config_.empty() && config_ != "{}") {
                    j["config"] = json::parse(config_);
                } else {
                    j["config"] = json::object();
                }
            } catch (const std::exception&) {
                j["config"] = json::object();
            }
            
            // 수집 설정
            j["polling_interval"] = polling_interval_;
            j["timeout"] = timeout_;
            j["retry_count"] = retry_count_;
            
            // 상태 정보
            j["is_enabled"] = is_enabled_;
            
            if (installation_date_.has_value()) {
                j["installation_date"] = dateToString(installation_date_.value());
            }
            if (last_maintenance_.has_value()) {
                j["last_maintenance"] = dateToString(last_maintenance_.value());
            }
            
            // 메타데이터
            if (created_by_.has_value()) {
                j["created_by"] = created_by_.value();
            }
            j["created_at"] = timestampToString(created_at_);
            j["updated_at"] = timestampToString(updated_at_);
            
        } catch (const std::exception& e) {
            logger_->Error("DeviceEntity::toJson failed: " + std::string(e.what()));
        }
        
        return j;
    }
    
    /**
     * @brief JSON에서 역직렬화
     * @param data JSON 데이터
     * @return 성공 시 true
     */
    bool fromJson(const json& data) override {
        try {
            // 기본 식별자
            if (data.contains("id")) {
                id_ = data["id"].get<int>();
            }
            if (data.contains("tenant_id")) {
                setTenantId(data["tenant_id"].get<int>());
            }
            if (data.contains("site_id")) {
                setSiteId(data["site_id"].get<int>());
            }
            if (data.contains("device_group_id")) {
                setDeviceGroupId(data["device_group_id"].get<int>());
            }
            if (data.contains("edge_server_id")) {
                setEdgeServerId(data["edge_server_id"].get<int>());
            }
            
            // 디바이스 기본 정보
            if (data.contains("name")) {
                setName(data["name"].get<std::string>());
            }
            if (data.contains("description")) {
                setDescription(data["description"].get<std::string>());
            }
            if (data.contains("device_type")) {
                setDeviceType(data["device_type"].get<std::string>());
            }
            if (data.contains("manufacturer")) {
                setManufacturer(data["manufacturer"].get<std::string>());
            }
            if (data.contains("model")) {
                setModel(data["model"].get<std::string>());
            }
            if (data.contains("serial_number")) {
                setSerialNumber(data["serial_number"].get<std::string>());
            }
            
            // 통신 설정
            if (data.contains("protocol_type")) {
                setProtocolType(data["protocol_type"].get<std::string>());
            }
            if (data.contains("endpoint")) {
                setEndpoint(data["endpoint"].get<std::string>());
            }
            if (data.contains("config")) {
                setConfig(data["config"].dump());
            }
            
            // 수집 설정
            if (data.contains("polling_interval")) {
                setPollingInterval(data["polling_interval"].get<int>());
            }
            if (data.contains("timeout")) {
                setTimeout(data["timeout"].get<int>());
            }
            if (data.contains("retry_count")) {
                setRetryCount(data["retry_count"].get<int>());
            }
            
            // 상태 정보
            if (data.contains("is_enabled")) {
                setEnabled(data["is_enabled"].get<bool>());
            }
            
            markModified();
            return true;
            
        } catch (const std::exception& e) {
            logger_->Error("DeviceEntity::fromJson failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }
    
    /**
     * @brief 엔티티 문자열 표현
     * @return 엔티티 정보 문자열
     */
    std::string toString() const override {
        std::ostringstream oss;
        oss << "DeviceEntity[";
        oss << "id=" << id_;
        oss << ", name=" << name_;
        oss << ", protocol=" << protocol_type_;
        oss << ", endpoint=" << endpoint_;
        oss << ", enabled=" << (is_enabled_ ? "true" : "false");
        oss << ", device_type=" << device_type_;
        oss << "]";
        return oss.str();
    }
    
    /**
     * @brief 테이블명 조회
     * @return 테이블명
     */
    std::string getTableName() const override { 
        return "devices"; 
    }

    // =======================================================================
    // 기본 속성 접근자 (스키마와 1:1 매핑)
    // =======================================================================
    
    // 관계 정보
    int getTenantId() const { return tenant_id_; }
    void setTenantId(int tenant_id) { 
        tenant_id_ = tenant_id; 
        markModified(); 
    }
    
    int getSiteId() const { return site_id_; }
    void setSiteId(int site_id) { 
        site_id_ = site_id; 
        markModified(); 
    }
    
    std::optional<int> getDeviceGroupId() const { return device_group_id_; }
    void setDeviceGroupId(const std::optional<int>& device_group_id) { 
        device_group_id_ = device_group_id; 
        markModified(); 
    }
    void setDeviceGroupId(int device_group_id) { 
        device_group_id_ = device_group_id; 
        markModified(); 
    }
    
    std::optional<int> getEdgeServerId() const { return edge_server_id_; }
    void setEdgeServerId(const std::optional<int>& edge_server_id) { 
        edge_server_id_ = edge_server_id; 
        markModified(); 
    }
    void setEdgeServerId(int edge_server_id) { 
        edge_server_id_ = edge_server_id; 
        markModified(); 
    }
    
    // 디바이스 기본 정보
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { 
        name_ = name; 
        markModified(); 
    }
    
    const std::string& getDescription() const { return description_; }
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified(); 
    }
    
    const std::string& getDeviceType() const { return device_type_; }
    void setDeviceType(const std::string& device_type) { 
        device_type_ = device_type; 
        markModified(); 
    }
    
    const std::string& getManufacturer() const { return manufacturer_; }
    void setManufacturer(const std::string& manufacturer) { 
        manufacturer_ = manufacturer; 
        markModified(); 
    }
    
    const std::string& getModel() const { return model_; }
    void setModel(const std::string& model) { 
        model_ = model; 
        markModified(); 
    }
    
    const std::string& getSerialNumber() const { return serial_number_; }
    void setSerialNumber(const std::string& serial_number) { 
        serial_number_ = serial_number; 
        markModified(); 
    }
    
    // 통신 설정
    const std::string& getProtocolType() const { return protocol_type_; }
    void setProtocolType(const std::string& protocol_type) { 
        protocol_type_ = protocol_type; 
        markModified(); 
    }
    
    const std::string& getEndpoint() const { return endpoint_; }
    void setEndpoint(const std::string& endpoint) { 
        endpoint_ = endpoint; 
        markModified(); 
    }
    
    const std::string& getConfig() const { return config_; }
    void setConfig(const std::string& config) { 
        config_ = config; 
        markModified(); 
    }
    
    // JSON config 헬퍼
    json getConfigAsJson() const {
        try {
            if (!config_.empty() && config_ != "{}") {
                return json::parse(config_);
            }
        } catch (const std::exception& e) {
            logger_->Warn("DeviceEntity::getConfigAsJson failed: " + std::string(e.what()));
        }
        return json::object();
    }
    
    void setConfigAsJson(const json& config) {
        config_ = config.dump();
        markModified();
    }
    
    // 수집 설정
    int getPollingInterval() const { return polling_interval_; }
    void setPollingInterval(int polling_interval) { 
        if (polling_interval > 0) {
            polling_interval_ = polling_interval; 
            markModified(); 
        }
    }
    
    int getTimeout() const { return timeout_; }
    void setTimeout(int timeout) { 
        if (timeout > 0) {
            timeout_ = timeout; 
            markModified(); 
        }
    }
    
    int getRetryCount() const { return retry_count_; }
    void setRetryCount(int retry_count) { 
        if (retry_count >= 0) {
            retry_count_ = retry_count; 
            markModified(); 
        }
    }
    
    // 상태 정보
    bool isEnabled() const { return is_enabled_; }
    void setEnabled(bool is_enabled) { 
        is_enabled_ = is_enabled; 
        markModified(); 
    }
    
    std::optional<std::chrono::system_clock::time_point> getInstallationDate() const { 
        return installation_date_; 
    }
    void setInstallationDate(const std::optional<std::chrono::system_clock::time_point>& installation_date) { 
        installation_date_ = installation_date; 
        markModified(); 
    }
    void setInstallationDate(const std::chrono::system_clock::time_point& installation_date) { 
        installation_date_ = installation_date; 
        markModified(); 
    }
    
    std::optional<std::chrono::system_clock::time_point> getLastMaintenance() const { 
        return last_maintenance_; 
    }
    void setLastMaintenance(const std::optional<std::chrono::system_clock::time_point>& last_maintenance) { 
        last_maintenance_ = last_maintenance; 
        markModified(); 
    }
    void setLastMaintenance(const std::chrono::system_clock::time_point& last_maintenance) { 
        last_maintenance_ = last_maintenance; 
        markModified(); 
    }
    
    std::optional<int> getCreatedBy() const { return created_by_; }
    void setCreatedBy(const std::optional<int>& created_by) { 
        created_by_ = created_by; 
        markModified(); 
    }
    void setCreatedBy(int created_by) { 
        created_by_ = created_by; 
        markModified(); 
    }

    // =======================================================================
    // 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 유효성 검사
     */
    bool isValid() const override {
        if (!BaseEntity<DeviceEntity>::isValid()) {
            return false;
        }
        
        // 필수 필드 검사 (NOT NULL 컬럼들)
        if (tenant_id_ <= 0) return false;
        if (site_id_ <= 0) return false;
        if (name_.empty()) return false;
        if (device_type_.empty()) return false;
        if (protocol_type_.empty()) return false;
        if (endpoint_.empty()) return false;
        if (config_.empty()) return false;
        
        // 값 범위 검사
        if (polling_interval_ <= 0) return false;
        if (timeout_ <= 0) return false;
        if (retry_count_ < 0) return false;
        
        return true;
    }
    
    /**
     * @brief 프로토콜별 설정 추출
     */
    json extractProtocolConfig() const {
        return getConfigAsJson();
    }
    
    /**
     * @brief 디바이스 타입별 기본 설정 적용
     */
    void applyDeviceTypeDefaults() {
        std::string type_lower = device_type_;
        std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(), ::tolower);
        
        if (type_lower == "plc") {
            if (polling_interval_ == 0) polling_interval_ = 1000;  // 1초
            if (timeout_ == 0) timeout_ = 5000;                   // 5초
            if (retry_count_ == 0) retry_count_ = 3;              // 3회
        } else if (type_lower == "sensor") {
            if (polling_interval_ == 0) polling_interval_ = 5000; // 5초
            if (timeout_ == 0) timeout_ = 3000;                   // 3초
            if (retry_count_ == 0) retry_count_ = 2;              // 2회
        } else if (type_lower == "gateway") {
            if (polling_interval_ == 0) polling_interval_ = 2000; // 2초
            if (timeout_ == 0) timeout_ = 10000;                  // 10초
            if (retry_count_ == 0) retry_count_ = 5;              // 5회
        }
        
        markModified();
    }
    
    /**
     * @brief 프로토콜별 기본 설정 적용
     */
    void applyProtocolDefaults() {
        json config = getConfigAsJson();
        
        std::string protocol_lower = protocol_type_;
        std::transform(protocol_lower.begin(), protocol_lower.end(), protocol_lower.begin(), ::tolower);
        
        if (protocol_lower.find("modbus") != std::string::npos) {
            if (!config.contains("unit_id")) config["unit_id"] = 1;
            if (!config.contains("function_code")) config["function_code"] = 3;
        } else if (protocol_lower.find("mqtt") != std::string::npos) {
            if (!config.contains("qos")) config["qos"] = 1;
            if (!config.contains("keep_alive")) config["keep_alive"] = 60;
        } else if (protocol_lower.find("bacnet") != std::string::npos) {
            if (!config.contains("device_id")) config["device_id"] = 1000;
            if (!config.contains("port")) config["port"] = 47808;
        }
        
        setConfigAsJson(config);
    }

private:
    // =======================================================================
    // 멤버 변수들 (스키마와 1:1 매핑)
    // =======================================================================
    
    // 관계 정보
    int tenant_id_;                         // NOT NULL, FOREIGN KEY to tenants.id
    int site_id_;                           // NOT NULL, FOREIGN KEY to sites.id
    std::optional<int> device_group_id_;    // FOREIGN KEY to device_groups.id
    std::optional<int> edge_server_id_;     // FOREIGN KEY to edge_servers.id
    
    // 디바이스 기본 정보
    std::string name_;                      // VARCHAR(100) NOT NULL
    std::string description_;               // TEXT
    std::string device_type_;               // VARCHAR(50) NOT NULL (PLC, HMI, SENSOR, GATEWAY, METER, CONTROLLER)
    std::string manufacturer_;              // VARCHAR(100)
    std::string model_;                     // VARCHAR(100)
    std::string serial_number_;             // VARCHAR(100)
    
    // 통신 설정
    std::string protocol_type_;             // VARCHAR(50) NOT NULL (MODBUS_TCP, MODBUS_RTU, MQTT, BACNET, OPCUA)
    std::string endpoint_;                  // VARCHAR(255) NOT NULL (IP:Port 또는 연결 문자열)
    std::string config_;                    // TEXT NOT NULL (JSON 형태 프로토콜별 설정)
    
    // 수집 설정
    int polling_interval_;                  // INTEGER DEFAULT 1000
    int timeout_;                           // INTEGER DEFAULT 3000
    int retry_count_;                       // INTEGER DEFAULT 3
    
    // 상태 정보
    bool is_enabled_;                       // INTEGER DEFAULT 1
    std::optional<std::chrono::system_clock::time_point> installation_date_;  // DATE
    std::optional<std::chrono::system_clock::time_point> last_maintenance_;   // DATE
    
    // 메타데이터
    std::optional<int> created_by_;         // INTEGER, FOREIGN KEY to users.id

    // =======================================================================
    // Repository 접근 및 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief Repository 인스턴스 얻기
     * @return DeviceRepository 포인터 (없으면 nullptr)
     */
    std::shared_ptr<Repositories::DeviceRepository> getRepository() const {
        try {
            // Repository 패턴: RepositoryFactory 사용
            return Repositories::RepositoryFactory::getInstance().getDeviceRepository();
        } catch (const std::exception& e) {
            logger_->Error("DeviceEntity::getRepository failed: " + std::string(e.what()));
            return nullptr;
        }
    }
    
    /**
     * @brief 날짜를 문자열로 변환
     */
    std::string dateToString(const std::chrono::system_clock::time_point& date) const {
        auto time_t = std::chrono::system_clock::to_time_t(date);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
        return oss.str();
    }
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_ENTITY_H