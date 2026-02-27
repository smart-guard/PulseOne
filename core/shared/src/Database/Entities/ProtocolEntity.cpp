/**
 * @file ProtocolEntity.cpp
 * @brief PulseOne ProtocolEntity 구현 - protocols 테이블 완전 매핑
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * DeviceEntity와 동일한 패턴:
 * - BaseEntity 순수 가상 함수 구현
 * - Repository 패턴 유지 (순환 참조 방지)
 * - JSON 지원 기능 구현
 */

#include "Database/Entities/ProtocolEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ProtocolRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현
// =============================================================================

ProtocolEntity::ProtocolEntity() 
    : BaseEntity<ProtocolEntity>()
    , protocol_type_("")
    , display_name_("")
    , description_("")
    , default_port_(std::nullopt)
    , uses_serial_(false)
    , requires_broker_(false)
    , supported_operations_("[]")         // 빈 JSON 배열
    , supported_data_types_("[]")         // 빈 JSON 배열
    , connection_params_("{}")            // 빈 JSON 객체
    , default_polling_interval_(1000)
    , default_timeout_(5000)
    , max_concurrent_connections_(1)
    , is_enabled_(true)
    , is_deprecated_(false)
    , min_firmware_version_("")
    , category_("")
    , vendor_("")
    , standard_reference_("")
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now()) {
}

ProtocolEntity::ProtocolEntity(int protocol_id) 
    : ProtocolEntity() {  // 위임 생성자 사용
    setId(protocol_id);
}

ProtocolEntity::ProtocolEntity(const std::string& protocol_type) 
    : ProtocolEntity() {  // 위임 생성자 사용
    setProtocolType(protocol_type);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 - Repository 활용
// =============================================================================

bool ProtocolEntity::loadFromDatabase() {
    if (getId() <= 0 && protocol_type_.empty()) {
        if (logger_) {
            logger_->Error("ProtocolEntity::loadFromDatabase - Invalid protocol ID or type");
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getProtocolRepository();
        if (repo) {
            std::optional<ProtocolEntity> loaded;
            
            if (getId() > 0) {
                loaded = repo->findById(getId());
            } else if (!protocol_type_.empty()) {
                loaded = repo->findByType(protocol_type_);
            }
            
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("ProtocolEntity - Loaded protocol: " + display_name_ + 
                                " (type: " + protocol_type_ + ")");
                }
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ProtocolEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool ProtocolEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("ProtocolEntity::saveToDatabase - Invalid protocol data. "
                         "Required fields: protocol_type=" + protocol_type_ + 
                         ", display_name=" + display_name_);
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getProtocolRepository();
        if (repo) {
            // 저장 전 타임스탬프 업데이트
            if (getId() <= 0) {
                created_at_ = std::chrono::system_clock::now();
            }
            updated_at_ = std::chrono::system_clock::now();
            
            bool success = repo->save(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("ProtocolEntity - Saved protocol: " + display_name_ + 
                                " (ID: " + std::to_string(getId()) + 
                                ", type: " + protocol_type_ + ")");
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ProtocolEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool ProtocolEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("ProtocolEntity::deleteFromDatabase - Invalid protocol ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getProtocolRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                markDeleted();
                if (logger_) {
                    logger_->Info("ProtocolEntity - Deleted protocol: " + display_name_ + 
                                " (ID: " + std::to_string(getId()) + ")");
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ProtocolEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool ProtocolEntity::updateToDatabase() {
    if (getId() <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("ProtocolEntity::updateToDatabase - Invalid protocol data or ID. "
                         "ID=" + std::to_string(getId()) + ", Valid=" + (isValid() ? "true" : "false"));
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getProtocolRepository();
        if (repo) {
            // 업데이트 전 타임스탬프 갱신
            updated_at_ = std::chrono::system_clock::now();
            
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("ProtocolEntity - Updated protocol: " + display_name_ + 
                                " (type: " + protocol_type_ + ")");
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ProtocolEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// 헬퍼 메서드 구현
// =============================================================================

std::string ProtocolEntity::timestampToString(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::shared_ptr<Repositories::ProtocolRepository> ProtocolEntity::getRepository() const {
    auto& factory = RepositoryFactory::getInstance();
    return factory.getProtocolRepository();
}

// =============================================================================
// 비즈니스 로직 메서드 구현
// =============================================================================

bool ProtocolEntity::supportsOperation(const std::string& operation) const {
    try {
        json operations = getSupportedOperationsAsJson();
        if (operations.is_array()) {
            for (const auto& op : operations) {
                if (op.is_string() && op.get<std::string>() == operation) {
                    return true;
                }
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ProtocolEntity::supportsOperation failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool ProtocolEntity::supportsDataType(const std::string& data_type) const {
    try {
        json data_types = getSupportedDataTypesAsJson();
        if (data_types.is_array()) {
            for (const auto& dt : data_types) {
                if (dt.is_string() && dt.get<std::string>() == data_type) {
                    return true;
                }
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ProtocolEntity::supportsDataType failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool ProtocolEntity::validateConnectionParams(const json& params) const {
    try {
        json schema = getConnectionParamsAsJson();
        if (schema.empty() || !schema.is_object()) {
            return true; // 스키마가 없으면 모든 파라미터 허용
        }
        
        // 간단한 검증: 필수 필드가 있는지 확인
        for (const auto& [key, value] : schema.items()) {
            if (value.is_object() && value.contains("required") && value["required"].get<bool>()) {
                if (!params.contains(key)) {
                    if (logger_) {
                        logger_->Warn("ProtocolEntity::validateConnectionParams - Missing required parameter: " + key);
                    }
                    return false;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ProtocolEntity::validateConnectionParams failed: " + std::string(e.what()));
        }
        return false;
    }
}

json ProtocolEntity::createDefaultConnectionParams() const {
    try {
        json schema = getConnectionParamsAsJson();
        json defaults = json::object();
        
        if (schema.is_object()) {
            for (const auto& [key, value] : schema.items()) {
                if (value.is_object() && value.contains("default")) {
                    defaults[key] = value["default"];
                }
            }
        }
        
        // 공통 기본값들
        if (default_port_.has_value()) {
            defaults["port"] = default_port_.value();
        }
        defaults["timeout"] = default_timeout_;
        defaults["polling_interval"] = default_polling_interval_;
        
        return defaults;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ProtocolEntity::createDefaultConnectionParams failed: " + std::string(e.what()));
        }
        return json::object();
    }
}

void ProtocolEntity::applyProtocolDefaults() {
    std::string type_upper = protocol_type_;
    std::transform(type_upper.begin(), type_upper.end(), type_upper.begin(), ::toupper);
    
    if (type_upper.find("MODBUS_TCP") != std::string::npos) {
        if (!default_port_.has_value()) default_port_ = 502;
        if (category_.empty()) category_ = "industrial";
        if (vendor_.empty()) vendor_ = "Modbus Organization";
        uses_serial_ = false;
        requires_broker_ = false;
        
        if (supported_operations_ == "[]") {
            setSupportedOperationsAsJson(json::parse(R"(["read_coils", "read_discrete_inputs", "read_holding_registers", "read_input_registers", "write_single_coil", "write_single_register", "write_multiple_coils", "write_multiple_registers"])"));
        }
        
        if (supported_data_types_ == "[]") {
            setSupportedDataTypesAsJson(json::parse(R"(["boolean", "int16", "uint16", "int32", "uint32", "float32"])"));
        }
        
    } else if (type_upper.find("MQTT") != std::string::npos) {
        if (!default_port_.has_value()) default_port_ = 1883;
        if (category_.empty()) category_ = "iot";
        if (vendor_.empty()) vendor_ = "MQTT.org";
        uses_serial_ = false;
        requires_broker_ = true;
        
        if (supported_operations_ == "[]") {
            setSupportedOperationsAsJson(json::parse(R"(["publish", "subscribe", "unsubscribe"])"));
        }
        
        if (supported_data_types_ == "[]") {
            setSupportedDataTypesAsJson(json::parse(R"(["string", "json", "binary", "boolean", "integer", "float"])"));
        }
        
    } else if (type_upper.find("BACNET") != std::string::npos) {
        if (!default_port_.has_value()) default_port_ = 47808;
        if (category_.empty()) category_ = "building_automation";
        if (vendor_.empty()) vendor_ = "ASHRAE";
        uses_serial_ = false;
        requires_broker_ = false;
        
        if (supported_operations_ == "[]") {
            setSupportedOperationsAsJson(json::parse(R"(["read_property", "write_property", "read_property_multiple", "cov_notification"])"));
        }
        
        if (supported_data_types_ == "[]") {
            setSupportedDataTypesAsJson(json::parse(R"(["boolean", "unsigned", "integer", "real", "enumerated", "character_string", "bit_string"])"));
        }
        
    } else if (type_upper.find("OPCUA") != std::string::npos || type_upper.find("OPC_UA") != std::string::npos) {
        if (!default_port_.has_value()) default_port_ = 4840;
        if (category_.empty()) category_ = "industrial";
        if (vendor_.empty()) vendor_ = "OPC Foundation";
        uses_serial_ = false;
        requires_broker_ = false;
        
        if (supported_operations_ == "[]") {
            setSupportedOperationsAsJson(json::parse(R"(["read", "write", "browse", "subscribe", "call_method"])"));
        }
        
        if (supported_data_types_ == "[]") {
            setSupportedDataTypesAsJson(json::parse(R"(["boolean", "sbyte", "byte", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float", "double", "string", "datetime", "guid"])"));
        }
    }
    
    markModified();
    
    if (logger_) {
        logger_->Info("ProtocolEntity - Applied defaults for protocol type: " + protocol_type_ +
                    " (port: " + (default_port_.has_value() ? std::to_string(default_port_.value()) : "none") + 
                    ", category: " + category_ + ")");
    }
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne