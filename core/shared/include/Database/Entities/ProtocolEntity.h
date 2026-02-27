#ifndef PROTOCOL_ENTITY_H
#define PROTOCOL_ENTITY_H

/**
 * @file ProtocolEntity.h
 * @brief PulseOne 프로토콜 엔티티 - protocols 테이블 완전 매핑
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * protocols 테이블과 1:1 매핑:
 * - protocol_type, display_name, description
 * - 네트워크 정보 (default_port, uses_serial, requires_broker)
 * - 지원 기능 (supported_operations, supported_data_types)
 * - 설정 정보 (default_polling_interval, default_timeout)
 * - BaseEntity<ProtocolEntity> 상속 (CRTP)
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

// Forward declarations
namespace Database {
namespace Repositories {
    class ProtocolRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 프로토콜 엔티티 클래스 - protocols 테이블과 완전 일치
 * 
 * CREATE TABLE protocols (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     protocol_type VARCHAR(50) NOT NULL UNIQUE,
 *     display_name VARCHAR(100) NOT NULL,
 *     description TEXT,
 *     default_port INTEGER,
 *     uses_serial INTEGER DEFAULT 0,
 *     requires_broker INTEGER DEFAULT 0,
 *     supported_operations TEXT,
 *     supported_data_types TEXT,
 *     connection_params TEXT,
 *     default_polling_interval INTEGER DEFAULT 1000,
 *     default_timeout INTEGER DEFAULT 5000,
 *     max_concurrent_connections INTEGER DEFAULT 1,
 *     is_enabled INTEGER DEFAULT 1,
 *     is_deprecated INTEGER DEFAULT 0,
 *     min_firmware_version VARCHAR(20),
 *     category VARCHAR(50),
 *     vendor VARCHAR(100),
 *     standard_reference VARCHAR(100),
 *     created_at DATETIME DEFAULT (datetime('now', 'localtime')),
 *     updated_at DATETIME DEFAULT (datetime('now', 'localtime'))
 * );
 */
class ProtocolEntity : public BaseEntity<ProtocolEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    ProtocolEntity();
    explicit ProtocolEntity(int protocol_id);
    explicit ProtocolEntity(const std::string& protocol_type);
    virtual ~ProtocolEntity() = default;

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
            // 기본 정보
            j["id"] = getId();
            j["protocol_type"] = protocol_type_;
            j["display_name"] = display_name_;
            j["description"] = description_;
            
            // 네트워크 정보
            if (default_port_.has_value()) {
                j["default_port"] = default_port_.value();
            }
            j["uses_serial"] = uses_serial_;
            j["requires_broker"] = requires_broker_;
            
            // 지원 기능 (JSON 형태)
            j["supported_operations"] = getSupportedOperationsAsJson();
            j["supported_data_types"] = getSupportedDataTypesAsJson();
            j["connection_params"] = getConnectionParamsAsJson();
            
            // 설정 정보
            j["default_polling_interval"] = default_polling_interval_;
            j["default_timeout"] = default_timeout_;
            j["max_concurrent_connections"] = max_concurrent_connections_;
            
            // 상태 정보
            j["is_enabled"] = is_enabled_;
            j["is_deprecated"] = is_deprecated_;
            j["min_firmware_version"] = min_firmware_version_;
            
            // 분류 정보
            j["category"] = category_;
            j["vendor"] = vendor_;
            j["standard_reference"] = standard_reference_;
            
            // 타임스탬프
            j["created_at"] = timestampToString(created_at_);
            j["updated_at"] = timestampToString(updated_at_);
            
        } catch (const std::exception&) {
            // JSON 생성 실패 시 기본 객체 반환
        }
        
        return j;
    }
    
    bool fromJson(const json& data) override {
        try {
            // 기본 정보
            if (data.contains("id")) {
                setId(data["id"].get<int>());
            }
            if (data.contains("protocol_type")) {
                setProtocolType(data["protocol_type"].get<std::string>());
            }
            if (data.contains("display_name")) {
                setDisplayName(data["display_name"].get<std::string>());
            }
            if (data.contains("description")) {
                setDescription(data["description"].get<std::string>());
            }
            
            // 네트워크 정보
            if (data.contains("default_port")) {
                setDefaultPort(data["default_port"].get<int>());
            }
            if (data.contains("uses_serial")) {
                setUsesSerial(data["uses_serial"].get<bool>());
            }
            if (data.contains("requires_broker")) {
                setRequiresBroker(data["requires_broker"].get<bool>());
            }
            
            // 지원 기능
            if (data.contains("supported_operations")) {
                setSupportedOperationsAsJson(data["supported_operations"]);
            }
            if (data.contains("supported_data_types")) {
                setSupportedDataTypesAsJson(data["supported_data_types"]);
            }
            if (data.contains("connection_params")) {
                setConnectionParamsAsJson(data["connection_params"]);
            }
            
            // 설정 정보
            if (data.contains("default_polling_interval")) {
                setDefaultPollingInterval(data["default_polling_interval"].get<int>());
            }
            if (data.contains("default_timeout")) {
                setDefaultTimeout(data["default_timeout"].get<int>());
            }
            if (data.contains("max_concurrent_connections")) {
                setMaxConcurrentConnections(data["max_concurrent_connections"].get<int>());
            }
            
            // 상태 정보
            if (data.contains("is_enabled")) {
                setEnabled(data["is_enabled"].get<bool>());
            }
            if (data.contains("is_deprecated")) {
                setDeprecated(data["is_deprecated"].get<bool>());
            }
            if (data.contains("min_firmware_version")) {
                setMinFirmwareVersion(data["min_firmware_version"].get<std::string>());
            }
            
            // 분류 정보
            if (data.contains("category")) {
                setCategory(data["category"].get<std::string>());
            }
            if (data.contains("vendor")) {
                setVendor(data["vendor"].get<std::string>());
            }
            if (data.contains("standard_reference")) {
                setStandardReference(data["standard_reference"].get<std::string>());
            }
            
            return true;
            
        } catch (const std::exception&) {
            return false;
        }
    }
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "ProtocolEntity[";
        oss << "id=" << getId();
        oss << ", protocol_type=" << protocol_type_;
        oss << ", display_name=" << display_name_;
        oss << ", enabled=" << (is_enabled_ ? "true" : "false");
        oss << ", category=" << category_;
        if (default_port_.has_value()) {
            oss << ", port=" << default_port_.value();
        }
        oss << "]";
        return oss.str();
    }
    
    std::string getTableName() const override { 
        return "protocols"; 
    }

    // =======================================================================
    // 기본 속성 접근자
    // =======================================================================
    
    // 기본 정보
    const std::string& getProtocolType() const { return protocol_type_; }
    void setProtocolType(const std::string& protocol_type) { 
        protocol_type_ = protocol_type; 
        markModified();
    }
    
    const std::string& getDisplayName() const { return display_name_; }
    void setDisplayName(const std::string& display_name) { 
        display_name_ = display_name; 
        markModified();
    }
    
    const std::string& getDescription() const { return description_; }
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified();
    }
    
    // 네트워크 정보
    std::optional<int> getDefaultPort() const { return default_port_; }
    void setDefaultPort(const std::optional<int>& default_port) { 
        default_port_ = default_port; 
        markModified();
    }
    void setDefaultPort(int default_port) { 
        default_port_ = default_port; 
        markModified();
    }
    
    bool usesSerial() const { return uses_serial_; }
    void setUsesSerial(bool uses_serial) { 
        uses_serial_ = uses_serial; 
        markModified();
    }
    
    bool requiresBroker() const { return requires_broker_; }
    void setRequiresBroker(bool requires_broker) { 
        requires_broker_ = requires_broker; 
        markModified();
    }
    
    // 지원 기능 (JSON 문자열로 저장)
    const std::string& getSupportedOperations() const { return supported_operations_; }
    void setSupportedOperations(const std::string& supported_operations) { 
        supported_operations_ = supported_operations; 
        markModified();
    }
    
    json getSupportedOperationsAsJson() const {
        try {
            if (!supported_operations_.empty()) {
                return json::parse(supported_operations_);
            }
        } catch (const std::exception&) {
            // 파싱 실패 시 빈 배열 반환
        }
        return json::array();
    }
    
    void setSupportedOperationsAsJson(const json& operations) {
        supported_operations_ = operations.dump();
        markModified();
    }
    
    const std::string& getSupportedDataTypes() const { return supported_data_types_; }
    void setSupportedDataTypes(const std::string& supported_data_types) { 
        supported_data_types_ = supported_data_types; 
        markModified();
    }
    
    json getSupportedDataTypesAsJson() const {
        try {
            if (!supported_data_types_.empty()) {
                return json::parse(supported_data_types_);
            }
        } catch (const std::exception&) {
            // 파싱 실패 시 빈 배열 반환
        }
        return json::array();
    }
    
    void setSupportedDataTypesAsJson(const json& data_types) {
        supported_data_types_ = data_types.dump();
        markModified();
    }
    
    const std::string& getConnectionParams() const { return connection_params_; }
    void setConnectionParams(const std::string& connection_params) { 
        connection_params_ = connection_params; 
        markModified();
    }
    
    json getConnectionParamsAsJson() const {
        try {
            if (!connection_params_.empty()) {
                return json::parse(connection_params_);
            }
        } catch (const std::exception&) {
            // 파싱 실패 시 빈 객체 반환
        }
        return json::object();
    }
    
    void setConnectionParamsAsJson(const json& params) {
        connection_params_ = params.dump();
        markModified();
    }
    
    // 설정 정보
    int getDefaultPollingInterval() const { return default_polling_interval_; }
    void setDefaultPollingInterval(int default_polling_interval) { 
        default_polling_interval_ = default_polling_interval; 
        markModified();
    }
    
    int getDefaultTimeout() const { return default_timeout_; }
    void setDefaultTimeout(int default_timeout) { 
        default_timeout_ = default_timeout; 
        markModified();
    }
    
    int getMaxConcurrentConnections() const { return max_concurrent_connections_; }
    void setMaxConcurrentConnections(int max_concurrent_connections) { 
        max_concurrent_connections_ = max_concurrent_connections; 
        markModified();
    }
    
    // 상태 정보
    bool isEnabled() const { return is_enabled_; }
    void setEnabled(bool is_enabled) { 
        is_enabled_ = is_enabled; 
        markModified();
    }
    
    bool isDeprecated() const { return is_deprecated_; }
    void setDeprecated(bool is_deprecated) { 
        is_deprecated_ = is_deprecated; 
        markModified();
    }
    
    const std::string& getMinFirmwareVersion() const { return min_firmware_version_; }
    void setMinFirmwareVersion(const std::string& min_firmware_version) { 
        min_firmware_version_ = min_firmware_version; 
        markModified();
    }
    
    // 분류 정보
    const std::string& getCategory() const { return category_; }
    void setCategory(const std::string& category) { 
        category_ = category; 
        markModified();
    }
    
    const std::string& getVendor() const { return vendor_; }
    void setVendor(const std::string& vendor) { 
        vendor_ = vendor; 
        markModified();
    }
    
    const std::string& getStandardReference() const { return standard_reference_; }
    void setStandardReference(const std::string& standard_reference) { 
        standard_reference_ = standard_reference; 
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
        if (!BaseEntity<ProtocolEntity>::isValid()) return false;
        
        // 필수 필드 검사
        if (protocol_type_.empty()) return false;
        if (display_name_.empty()) return false;
        
        // 카테고리 유효성 검사
        if (!category_.empty()) {
            static const std::vector<std::string> valid_categories = {
                "industrial", "iot", "building_automation", "network", "web"
            };
            if (std::find(valid_categories.begin(), valid_categories.end(), category_) == valid_categories.end()) {
                return false;
            }
        }
        
        // 설정값 유효성 검사
        if (default_polling_interval_ <= 0) return false;
        if (default_timeout_ <= 0) return false;
        if (max_concurrent_connections_ <= 0) return false;
        
        return true;
    }

    // =======================================================================
    // 비즈니스 로직 메서드들
    // =======================================================================
    
    /**
     * @brief 해당 프로토콜이 특정 연산을 지원하는지 확인
     * @param operation 연산 이름 ("read", "write", "subscribe" 등)
     * @return 지원하면 true
     */
    bool supportsOperation(const std::string& operation) const;
    
    /**
     * @brief 해당 프로토콜이 특정 데이터 타입을 지원하는지 확인
     * @param data_type 데이터 타입 ("boolean", "int16", "float32" 등)
     * @return 지원하면 true
     */
    bool supportsDataType(const std::string& data_type) const;
    
    /**
     * @brief 연결에 필요한 파라미터가 유효한지 검증
     * @param params 연결 파라미터 JSON
     * @return 유효하면 true
     */
    bool validateConnectionParams(const json& params) const;
    
    /**
     * @brief 기본 연결 파라미터 생성
     * @return 기본값이 설정된 연결 파라미터 JSON
     */
    json createDefaultConnectionParams() const;
    
    /**
     * @brief 프로토콜 타입 기반 기본값 설정
     */
    void applyProtocolDefaults();

    // =======================================================================
    // 헬퍼 메서드들
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& timestamp) const;
    
    // Repository 접근자
    std::shared_ptr<Repositories::ProtocolRepository> getRepository() const;
    
private:
    // =======================================================================
    // 멤버 변수들 - protocols 테이블과 1:1 매핑
    // =======================================================================
    
    // 기본 정보
    std::string protocol_type_;               // VARCHAR(50) NOT NULL UNIQUE
    std::string display_name_;                // VARCHAR(100) NOT NULL
    std::string description_;                 // TEXT
    
    // 네트워크 정보
    std::optional<int> default_port_;         // INTEGER
    bool uses_serial_;                        // INTEGER DEFAULT 0
    bool requires_broker_;                    // INTEGER DEFAULT 0
    
    // 지원 기능 (JSON 문자열로 저장)
    std::string supported_operations_;        // TEXT (JSON 배열)
    std::string supported_data_types_;        // TEXT (JSON 배열)
    std::string connection_params_;           // TEXT (JSON 객체)
    
    // 설정 정보
    int default_polling_interval_;            // INTEGER DEFAULT 1000
    int default_timeout_;                     // INTEGER DEFAULT 5000
    int max_concurrent_connections_;          // INTEGER DEFAULT 1
    
    // 상태 정보
    bool is_enabled_;                         // INTEGER DEFAULT 1
    bool is_deprecated_;                      // INTEGER DEFAULT 0
    std::string min_firmware_version_;        // VARCHAR(20)
    
    // 분류 정보
    std::string category_;                    // VARCHAR(50)
    std::string vendor_;                      // VARCHAR(100)
    std::string standard_reference_;          // VARCHAR(100)
    
    // 메타데이터
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // PROTOCOL_ENTITY_H