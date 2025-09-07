#ifndef DEVICE_ENTITY_H
#define DEVICE_ENTITY_H

/**
 * @file DeviceEntity.h
 * @brief PulseOne 디바이스 엔티티 - ProtocolRepository 동적 조회 완성본
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 🔥 현재 DB 스키마와 완전 일치:
 * - protocol_type → protocol_id (외래키)
 * - 새로운 컬럼들: polling_interval, timeout, retry_count
 * - BaseEntity<DeviceEntity> 상속 (CRTP)
 * - devices 테이블과 1:1 매핑
 * - ProtocolRepository를 통한 동적 프로토콜 조회
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <map>

namespace PulseOne {

// Forward declarations (순환 참조 방지)
namespace Database {
namespace Repositories {
    class DeviceRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 디바이스 엔티티 클래스 - 현재 DB 스키마와 완전 일치
 * 
 * 🔥 현재 DB 스키마 매핑:
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
 *     -- 프로토콜 설정 (외래키로 변경!)
 *     protocol_id INTEGER NOT NULL,           -- 🔥 변경됨!
 *     endpoint VARCHAR(255) NOT NULL,
 *     config TEXT NOT NULL,
 *     
 *     -- 수집 설정 (새로 추가!)
 *     polling_interval INTEGER DEFAULT 1000, -- 🔥 새로 추가!
 *     timeout INTEGER DEFAULT 3000,          -- 🔥 새로 추가!
 *     retry_count INTEGER DEFAULT 3,         -- 🔥 새로 추가!
 *     
 *     -- 상태 정보
 *     is_enabled INTEGER DEFAULT 1,
 *     installation_date DATE,
 *     last_maintenance DATE,
 *     
 *     created_by INTEGER,
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     
 *     FOREIGN KEY (protocol_id) REFERENCES protocols(id)
 * );
 */
class DeviceEntity : public BaseEntity<DeviceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    DeviceEntity();
    explicit DeviceEntity(int device_id);
    virtual ~DeviceEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화
    // =======================================================================
    
    json toJson() const override {
        json j;
        
        try {
            // 기본 식별자
            j["id"] = getId();
            j["tenant_id"] = tenant_id_;
            j["site_id"] = site_id_;
            
            if (device_group_id_.has_value()) {
                j["device_group_id"] = device_group_id_.value();
            }
            if (edge_server_id_.has_value()) {
                j["edge_server_id"] = edge_server_id_.value();
            }
            
            // 디바이스 정보
            j["name"] = name_;
            j["description"] = description_;
            j["device_type"] = device_type_;
            j["manufacturer"] = manufacturer_;
            j["model"] = model_;
            j["serial_number"] = serial_number_;
            
            // 🔥 프로토콜 설정 (protocol_id로 변경)
            j["protocol_id"] = protocol_id_;
            j["endpoint"] = endpoint_;
            j["config"] = getConfigAsJson();
            
            // 🔥 수집 설정 (새로 추가)
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
            
        } catch (const std::exception&) {
            // JSON 생성 실패 시 기본 객체 반환
        }
        
        return j;
    }
    
    bool fromJson(const json& data) override {
        try {
            // 기본 식별자
            if (data.contains("id")) {
                setId(data["id"].get<int>());
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
            
            // 디바이스 정보
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
            
            // 🔥 프로토콜 설정 (protocol_id로 변경)
            if (data.contains("protocol_id")) {
                setProtocolId(data["protocol_id"].get<int>());
            }
            if (data.contains("endpoint")) {
                setEndpoint(data["endpoint"].get<std::string>());
            }
            if (data.contains("config")) {
                setConfig(data["config"].dump());
            }
            
            // 🔥 수집 설정 (새로 추가)
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
            
            return true;
            
        } catch (const std::exception&) {
            return false;
        }
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "DeviceEntity[";
        oss << "id=" << getId();
        oss << ", name=" << name_;
        oss << ", protocol_id=" << protocol_id_;
        oss << ", endpoint=" << endpoint_;
        oss << ", enabled=" << (is_enabled_ ? "true" : "false");
        oss << ", device_type=" << device_type_;
        oss << ", polling_interval=" << polling_interval_;
        oss << "]";
        return oss.str();
    }
    
    std::string getTableName() const override { 
        return "devices"; 
    }

    // =======================================================================
    // 기본 속성 접근자
    // =======================================================================
    
    // ID 및 관계 정보
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
    
    // 🔥 프로토콜 설정 (protocol_id로 변경)
    int getProtocolId() const { return protocol_id_; }
    void setProtocolId(int protocol_id) { 
        protocol_id_ = protocol_id; 
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
        } catch (const std::exception&) {
            // 파싱 실패 시 빈 객체 반환
        }
        return json::object();
    }
    
    void setConfigAsJson(const json& config) {
        config_ = config.dump();
        markModified();
    }
    
    // 🔥 수집 설정 (새로 추가)
    int getPollingInterval() const { return polling_interval_; }
    void setPollingInterval(int polling_interval) { 
        polling_interval_ = polling_interval; 
        markModified();
    }
    
    int getTimeout() const { return timeout_; }
    void setTimeout(int timeout) { 
        timeout_ = timeout; 
        markModified();
    }
    
    int getRetryCount() const { return retry_count_; }
    void setRetryCount(int retry_count) { 
        retry_count_ = retry_count; 
        markModified();
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
    
    // 타임스탬프
    std::chrono::system_clock::time_point getCreatedAt() const { return created_at_; }
    void setCreatedAt(const std::chrono::system_clock::time_point& created_at) { 
        created_at_ = created_at; 
        markModified();
    }
    
    std::chrono::system_clock::time_point getUpdatedAt() const { return updated_at_; }
    void setUpdatedAt(const std::chrono::system_clock::time_point& updated_at) { 
        updated_at_ = updated_at; 
        markModified();
    }

    // =======================================================================
    // 유효성 검사
    // =======================================================================
    
    bool isValid() const override {
        if (!BaseEntity<DeviceEntity>::isValid()) return false;
        
        // 필수 필드 검사 (NOT NULL 컬럼들)
        if (tenant_id_ <= 0) return false;
        if (site_id_ <= 0) return false;
        if (name_.empty()) return false;
        if (device_type_.empty()) return false;
        if (protocol_id_ <= 0) return false;  // 🔥 변경됨!
        if (endpoint_.empty()) return false;
        if (config_.empty()) return false;
        
        // 🔥 수집 설정 유효성 검사 (새로 추가)
        if (polling_interval_ <= 0) return false;
        if (timeout_ <= 0) return false;
        if (retry_count_ < 0) return false;
        
        return true;
    }

    // =======================================================================
    // 비즈니스 로직 메서드들
    // =======================================================================
    
    void applyDeviceTypeDefaults();
    void applyProtocolDefaults();
    void setOptimalPollingForProtocol();
    void setOptimalTimeoutForEndpoint();
    bool isLocalEndpoint() const;
    bool isLANEndpoint() const;
    
    // =======================================================================
    // 🔥 프로토콜 정보 조회 및 설정 (ProtocolRepository 활용)
    // =======================================================================
    
    // 기본 프로토콜 정보 (deprecated 선언 + 새 구현)
    [[deprecated("Use getProtocolId() instead")]]
    std::string getProtocolType() const;
    
    [[deprecated("Use setProtocolId() instead")]]
    void setProtocolType(const std::string& protocol_type);
    
    // 추가 프로토콜 정보 헬퍼 메서드들
    std::string getProtocolDisplayName() const;
    int getProtocolDefaultPort() const;
    bool isProtocolSerial() const;
    bool requiresBroker() const;
    std::string getProtocolCategory() const;

    // =======================================================================
    // 헬퍼 메서드들
    // =======================================================================
    
    std::string dateToString(const std::chrono::system_clock::time_point& date) const;
    std::string timestampToString(const std::chrono::system_clock::time_point& timestamp) const;
    
    // Repository 접근자
    std::shared_ptr<Repositories::DeviceRepository> getRepository() const;
    
private:
    // =======================================================================
    // 멤버 변수들 - 현재 DB 스키마와 1:1 매핑
    // =======================================================================
    
    // 관계 정보
    int tenant_id_;                                 // NOT NULL
    int site_id_;                                   // NOT NULL
    std::optional<int> device_group_id_;            
    std::optional<int> edge_server_id_;             
    
    // 디바이스 기본 정보
    std::string name_;                              // VARCHAR(100) NOT NULL
    std::string description_;                       // TEXT
    std::string device_type_;                       // VARCHAR(50) NOT NULL
    std::string manufacturer_;                      // VARCHAR(100)
    std::string model_;                             // VARCHAR(100)
    std::string serial_number_;                     // VARCHAR(100)
    
    // 🔥 프로토콜 설정 (protocol_id로 변경)
    int protocol_id_;                               // INTEGER NOT NULL, FOREIGN KEY
    std::string endpoint_;                          // VARCHAR(255) NOT NULL
    std::string config_;                            // TEXT NOT NULL
    
    // 🔥 수집 설정 (새로 추가)
    int polling_interval_;                          // INTEGER DEFAULT 1000
    int timeout_;                                   // INTEGER DEFAULT 3000
    int retry_count_;                               // INTEGER DEFAULT 3
    
    // 상태 정보
    bool is_enabled_;                               // INTEGER DEFAULT 1
    std::optional<std::chrono::system_clock::time_point> installation_date_;
    std::optional<std::chrono::system_clock::time_point> last_maintenance_;
    
    // 메타데이터
    std::optional<int> created_by_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_ENTITY_H