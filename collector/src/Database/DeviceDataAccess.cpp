// =============================================================================
// collector/src/Database/DeviceDataAccess.cpp
// PulseOne 디바이스 데이터 액세스 구현 (팩토리 패턴 포함)
// =============================================================================

#include "Database/DeviceDataAccess.h"
#include "Database/DataAccessManager.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <pqxx/pqxx>

namespace PulseOne {
namespace Database {

// =============================================================================
// 내부 유틸리티 함수
// =============================================================================

// SQL 인젝션 방지를 위한 문자열 이스케이프
static std::string EscapeString(const std::string& input) {
    std::string result;
    result.reserve(input.size() * 2);
    
    for (char c : input) {
        if (c == '\'') {
            result += "''";  // Single quote escape
        } else if (c == '\\') {
            result += "\\\\";  // Backslash escape
        } else {
            result += c;
        }
    }
    
    return result;
}

// =============================================================================
// DeviceDataAccess 생성자 및 소멸자
// =============================================================================

DeviceDataAccess::DeviceDataAccess(std::shared_ptr<DatabaseManager> db_manager,
                                   const std::string& tenant_code)
    : db_manager_(db_manager)
    , tenant_code_(tenant_code)
    , schema_name_(GetSchemaName(tenant_code))
    , logger_(LogManager::getInstance()) {
    
    logger_.Info("DeviceDataAccess created for tenant: " + tenant_code);
}

DeviceDataAccess::~DeviceDataAccess() {
    logger_.Info("DeviceDataAccess destroyed for tenant: " + tenant_code_);
}

// =============================================================================
// 유틸리티 메소드 구현
// =============================================================================

std::string DeviceDataAccess::GetSchemaName(const std::string& tenant_code) {
    return "tenant_" + tenant_code;
}

std::string DeviceDataAccess::GetTableName(const std::string& table_name) {
    return schema_name_ + "." + table_name;
}

std::string DeviceDataAccess::UUIDToString(const UUID& uuid) {
    // UUID가 std::string으로 정의되어 있음
    return uuid;
}

UUID DeviceDataAccess::StringToUUID(const std::string& uuid_str) {
    // UUID가 std::string으로 정의되어 있음
    return uuid_str;
}

std::string DeviceDataAccess::TimestampToString(const Timestamp& timestamp) {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

Timestamp DeviceDataAccess::StringToTimestamp(const std::string& timestamp_str) {
    std::tm tm = {};
    std::stringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::vector<std::string> DeviceDataAccess::JsonArrayToStringVector(const json& json_array) {
    std::vector<std::string> result;
    if (json_array.is_array()) {
        for (const auto& item : json_array) {
            if (item.is_string()) {
                result.push_back(item.get<std::string>());
            }
        }
    }
    return result;
}

json DeviceDataAccess::StringVectorToJsonArray(const std::vector<std::string>& string_vector) {
    json result = json::array();
    for (const auto& str : string_vector) {
        result.push_back(str);
    }
    return result;
}

// =============================================================================
// 테넌트 및 공장 관리
// =============================================================================

std::optional<TenantInfo> DeviceDataAccess::GetTenantInfo(const std::string& tenant_code) {
    try {
        std::string query = "SELECT * FROM public.tenants WHERE tenant_code = '" + 
                           EscapeString(tenant_code) + "'";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        if (result.empty()) {
            return std::nullopt;
        }
        
        TenantInfo info;
        const auto& row = result[0];
        
        info.tenant_id = StringToUUID(row["tenant_id"].as<std::string>());
        info.company_name = row["company_name"].as<std::string>();
        info.company_code = row["company_code"].as<std::string>();
        info.subscription_status = row["subscription_status"].as<std::string>();
        info.is_active = row["is_active"].as<bool>();
        info.created_at = StringToTimestamp(row["created_at"].as<std::string>());
        
        if (!row["settings"].is_null()) {
            info.settings = json::parse(row["settings"].as<std::string>());
        }
        if (!row["features"].is_null()) {
            info.features = json::parse(row["features"].as<std::string>());
        }
        
        return info;
        
    } catch (const std::exception& e) {
        logger_.Error("GetTenantInfo failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<FactoryInfo> DeviceDataAccess::GetFactories(bool active_only) {
    std::vector<FactoryInfo> factories;
    
    try {
        std::string query = "SELECT * FROM " + GetTableName("factories");
        if (active_only) {
            query += " WHERE is_active = true";
        }
        query += " ORDER BY name";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        for (const auto& row : result) {
            FactoryInfo factory;
            
            factory.factory_id = StringToUUID(row["factory_id"].as<std::string>());
            factory.name = row["name"].as<std::string>();
            factory.code = row["code"].as<std::string>();
            factory.description = row["description"].as<std::string>();
            factory.location = row["location"].as<std::string>();
            factory.timezone = row["timezone"].as<std::string>();
            factory.manager_name = row["manager_name"].as<std::string>();
            factory.manager_email = row["manager_email"].as<std::string>();
            factory.manager_phone = row["manager_phone"].as<std::string>();
            
            if (!row["edge_server_id"].is_null()) {
                factory.edge_server_id = StringToUUID(row["edge_server_id"].as<std::string>());
            }
            
            factory.is_active = row["is_active"].as<bool>();
            factory.created_at = StringToTimestamp(row["created_at"].as<std::string>());
            factory.updated_at = StringToTimestamp(row["updated_at"].as<std::string>());
            
            factories.push_back(factory);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("GetFactories failed: " + std::string(e.what()));
    }
    
    return factories;
}

std::optional<FactoryInfo> DeviceDataAccess::GetFactory(const UUID& factory_id) {
    try {
        std::string query = "SELECT * FROM " + GetTableName("factories") + 
                           " WHERE factory_id = '" + UUIDToString(factory_id) + "'";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        if (result.empty()) {
            return std::nullopt;
        }
        
        const auto& row = result[0];
        FactoryInfo factory;
        
        factory.factory_id = StringToUUID(row["factory_id"].as<std::string>());
        factory.name = row["name"].as<std::string>();
        factory.code = row["code"].as<std::string>();
        factory.description = row["description"].as<std::string>();
        factory.location = row["location"].as<std::string>();
        factory.timezone = row["timezone"].as<std::string>();
        factory.manager_name = row["manager_name"].as<std::string>();
        factory.manager_email = row["manager_email"].as<std::string>();
        factory.manager_phone = row["manager_phone"].as<std::string>();
        
        if (!row["edge_server_id"].is_null()) {
            factory.edge_server_id = StringToUUID(row["edge_server_id"].as<std::string>());
        }
        
        factory.is_active = row["is_active"].as<bool>();
        factory.created_at = StringToTimestamp(row["created_at"].as<std::string>());
        factory.updated_at = StringToTimestamp(row["updated_at"].as<std::string>());
        
        return factory;
        
    } catch (const std::exception& e) {
        logger_.Error("GetFactory failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DeviceDataAccess::CreateFactory(const FactoryInfo& factory) {
    try {
        std::ostringstream sql;
        sql << "INSERT INTO " << GetTableName("factories") << " ("
            << "factory_id, name, code, description, location, timezone, "
            << "manager_name, manager_email, manager_phone, edge_server_id, "
            << "is_active, created_at, updated_at"
            << ") VALUES (";
        
        sql << "'" << UUIDToString(factory.factory_id) << "', ";
        sql << "'" << EscapeString(factory.name) << "', ";
        sql << "'" << EscapeString(factory.code) << "', ";
        sql << "'" << EscapeString(factory.description) << "', ";
        sql << "'" << EscapeString(factory.location) << "', ";
        sql << "'" << EscapeString(factory.timezone) << "', ";
        sql << "'" << EscapeString(factory.manager_name) << "', ";
        sql << "'" << EscapeString(factory.manager_email) << "', ";
        sql << "'" << EscapeString(factory.manager_phone) << "', ";
        
        if (factory.edge_server_id.has_value()) {
            sql << "'" << UUIDToString(factory.edge_server_id.value()) << "', ";
        } else {
            sql << "NULL, ";
        }
        
        sql << (factory.is_active ? "true" : "false") << ", ";
        sql << "NOW(), NOW())";
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Info("Created factory: " + factory.name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("CreateFactory failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::UpdateFactory(const FactoryInfo& factory) {
    try {
        std::ostringstream sql;
        sql << "UPDATE " << GetTableName("factories") << " SET "
            << "name = '" << EscapeString(factory.name) << "', "
            << "code = '" << EscapeString(factory.code) << "', "
            << "description = '" << EscapeString(factory.description) << "', "
            << "location = '" << EscapeString(factory.location) << "', "
            << "timezone = '" << EscapeString(factory.timezone) << "', "
            << "manager_name = '" << EscapeString(factory.manager_name) << "', "
            << "manager_email = '" << EscapeString(factory.manager_email) << "', "
            << "manager_phone = '" << EscapeString(factory.manager_phone) << "', ";
        
        if (factory.edge_server_id.has_value()) {
            sql << "edge_server_id = '" << UUIDToString(factory.edge_server_id.value()) << "', ";
        } else {
            sql << "edge_server_id = NULL, ";
        }
        
        sql << "is_active = " << (factory.is_active ? "true" : "false") << ", "
            << "updated_at = NOW() "
            << "WHERE factory_id = '" << UUIDToString(factory.factory_id) << "'";
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Info("Updated factory: " + factory.name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("UpdateFactory failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 디바이스 관리
// =============================================================================

std::vector<DeviceInfo> DeviceDataAccess::GetDevices(const UUID& factory_id, bool enabled_only) {
    std::vector<DeviceInfo> devices;
    
    try {
        std::string query = "SELECT * FROM " + GetTableName("devices") + " WHERE 1=1";
        
        if (!factory_id.empty()) {  // UUID는 std::string이므로 empty() 사용
            query += " AND factory_id = '" + UUIDToString(factory_id) + "'";
        }
        
        if (enabled_only) {
            query += " AND is_enabled = true";
        }
        
        query += " ORDER BY name";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        for (const auto& row : result) {
            DeviceInfo device;
            
            device.device_id = StringToUUID(row["device_id"].as<std::string>());
            device.factory_id = StringToUUID(row["factory_id"].as<std::string>());
            device.name = row["name"].as<std::string>();
            device.code = row["code"].as<std::string>();
            device.description = row["description"].as<std::string>();
            device.protocol_type = row["protocol_type"].as<std::string>();
            device.connection_string = row["connection_string"].as<std::string>();
            
            if (!row["connection_config"].is_null()) {
                device.connection_config = json::parse(row["connection_config"].as<std::string>());
            }
            
            device.is_enabled = row["is_enabled"].as<bool>();
            device.status = row["status"].as<std::string>();
            device.last_seen = StringToTimestamp(row["last_seen"].as<std::string>());
            device.polling_interval_ms = row["polling_interval_ms"].as<int>();
            device.timeout_ms = row["timeout_ms"].as<int>();
            device.retry_count = row["retry_count"].as<int>();
            
            if (!row["device_group_id"].is_null()) {
                device.device_group_id = StringToUUID(row["device_group_id"].as<std::string>());
            }
            
            if (!row["tags"].is_null()) {
                device.tags = JsonArrayToStringVector(json::parse(row["tags"].as<std::string>()));
            }
            
            if (!row["metadata"].is_null()) {
                device.metadata = json::parse(row["metadata"].as<std::string>());
            }
            
            device.created_at = StringToTimestamp(row["created_at"].as<std::string>());
            device.updated_at = StringToTimestamp(row["updated_at"].as<std::string>());
            
            devices.push_back(device);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("GetDevices failed: " + std::string(e.what()));
    }
    
    return devices;
}

std::optional<DeviceInfo> DeviceDataAccess::GetDevice(const UUID& device_id) {
    try {
        std::string query = "SELECT * FROM " + GetTableName("devices") + 
                           " WHERE device_id = '" + UUIDToString(device_id) + "'";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        if (result.empty()) {
            return std::nullopt;
        }
        
        const auto& row = result[0];
        DeviceInfo device;
        
        device.device_id = StringToUUID(row["device_id"].as<std::string>());
        device.factory_id = StringToUUID(row["factory_id"].as<std::string>());
        device.name = row["name"].as<std::string>();
        device.code = row["code"].as<std::string>();
        device.description = row["description"].as<std::string>();
        device.protocol_type = row["protocol_type"].as<std::string>();
        device.connection_string = row["connection_string"].as<std::string>();
        
        if (!row["connection_config"].is_null()) {
            device.connection_config = json::parse(row["connection_config"].as<std::string>());
        }
        
        device.is_enabled = row["is_enabled"].as<bool>();
        device.status = row["status"].as<std::string>();
        device.last_seen = StringToTimestamp(row["last_seen"].as<std::string>());
        device.polling_interval_ms = row["polling_interval_ms"].as<int>();
        device.timeout_ms = row["timeout_ms"].as<int>();
        device.retry_count = row["retry_count"].as<int>();
        
        if (!row["device_group_id"].is_null()) {
            device.device_group_id = StringToUUID(row["device_group_id"].as<std::string>());
        }
        
        if (!row["tags"].is_null()) {
            device.tags = JsonArrayToStringVector(json::parse(row["tags"].as<std::string>()));
        }
        
        if (!row["metadata"].is_null()) {
            device.metadata = json::parse(row["metadata"].as<std::string>());
        }
        
        device.created_at = StringToTimestamp(row["created_at"].as<std::string>());
        device.updated_at = StringToTimestamp(row["updated_at"].as<std::string>());
        
        return device;
        
    } catch (const std::exception& e) {
        logger_.Error("GetDevice failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DeviceDataAccess::CreateDevice(const DeviceInfo& device) {
    try {
        std::ostringstream sql;
        sql << "INSERT INTO " << GetTableName("devices") << " ("
            << "device_id, factory_id, name, code, description, protocol_type, "
            << "connection_string, connection_config, is_enabled, status, last_seen, "
            << "polling_interval_ms, timeout_ms, retry_count, device_group_id, "
            << "tags, metadata, created_at, updated_at"
            << ") VALUES (";
        
        sql << "'" << UUIDToString(device.device_id) << "', ";
        sql << "'" << UUIDToString(device.factory_id) << "', ";
        sql << "'" << EscapeString(device.name) << "', ";
        sql << "'" << EscapeString(device.code) << "', ";
        sql << "'" << EscapeString(device.description) << "', ";
        sql << "'" << EscapeString(device.protocol_type) << "', ";
        sql << "'" << EscapeString(device.connection_string) << "', ";
        sql << "'" << EscapeString(device.connection_config.dump()) << "', ";
        sql << (device.is_enabled ? "true" : "false") << ", ";
        sql << "'" << EscapeString(device.status) << "', ";
        sql << "'" << TimestampToString(device.last_seen) << "', ";
        sql << device.polling_interval_ms << ", ";
        sql << device.timeout_ms << ", ";
        sql << device.retry_count << ", ";
        
        if (device.device_group_id.has_value()) {
            sql << "'" << UUIDToString(device.device_group_id.value()) << "', ";
        } else {
            sql << "NULL, ";
        }
        
        sql << "'" << EscapeString(StringVectorToJsonArray(device.tags).dump()) << "', ";
        sql << "'" << EscapeString(device.metadata.dump()) << "', ";
        sql << "NOW(), NOW())";
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Info("Created device: " + device.name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("CreateDevice failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::UpdateDevice(const DeviceInfo& device) {
    try {
        std::ostringstream sql;
        sql << "UPDATE " << GetTableName("devices") << " SET "
            << "factory_id = '" << UUIDToString(device.factory_id) << "', "
            << "name = '" << EscapeString(device.name) << "', "
            << "code = '" << EscapeString(device.code) << "', "
            << "description = '" << EscapeString(device.description) << "', "
            << "protocol_type = '" << EscapeString(device.protocol_type) << "', "
            << "connection_string = '" << EscapeString(device.connection_string) << "', "
            << "connection_config = '" << EscapeString(device.connection_config.dump()) << "', "
            << "is_enabled = " << (device.is_enabled ? "true" : "false") << ", "
            << "status = '" << EscapeString(device.status) << "', "
            << "last_seen = '" << TimestampToString(device.last_seen) << "', "
            << "polling_interval_ms = " << device.polling_interval_ms << ", "
            << "timeout_ms = " << device.timeout_ms << ", "
            << "retry_count = " << device.retry_count << ", ";
        
        if (device.device_group_id.has_value()) {
            sql << "device_group_id = '" << UUIDToString(device.device_group_id.value()) << "', ";
        } else {
            sql << "device_group_id = NULL, ";
        }
        
        sql << "tags = '" << EscapeString(StringVectorToJsonArray(device.tags).dump()) << "', "
            << "metadata = '" << EscapeString(device.metadata.dump()) << "', "
            << "updated_at = NOW() "
            << "WHERE device_id = '" << UUIDToString(device.device_id) << "'";
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Info("Updated device: " + device.name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("UpdateDevice failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::DeleteDevice(const UUID& device_id) {
    try {
        std::string sql = "DELETE FROM " + GetTableName("devices") + 
                         " WHERE device_id = '" + UUIDToString(device_id) + "'";
        
        bool success = db_manager_->executeNonQueryPostgres(sql);
        
        if (success) {
            logger_.Info("Deleted device: " + UUIDToString(device_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DeleteDevice failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::UpdateDeviceStatus(const UUID& device_id, const std::string& status, const Timestamp& last_seen) {
    try {
        std::ostringstream sql;
        sql << "UPDATE " << GetTableName("devices") << " SET "
            << "status = '" << EscapeString(status) << "', "
            << "last_seen = '" << TimestampToString(last_seen) << "', "
            << "updated_at = NOW() "
            << "WHERE device_id = '" << UUIDToString(device_id) << "'";
        
        return db_manager_->executeNonQueryPostgres(sql.str());
        
    } catch (const std::exception& e) {
        logger_.Error("UpdateDeviceStatus failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 데이터 포인트 관리
// =============================================================================

std::vector<DataPointInfo> DeviceDataAccess::GetDataPoints(const UUID& device_id, bool enabled_only) {
    std::vector<DataPointInfo> datapoints;
    
    try {
        std::string query = "SELECT * FROM " + GetTableName("data_points") + 
                           " WHERE device_id = '" + UUIDToString(device_id) + "'";
        
        if (enabled_only) {
            query += " AND is_enabled = true";
        }
        
        query += " ORDER BY address";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        for (const auto& row : result) {
            DataPointInfo point;
            
            point.point_id = StringToUUID(row["point_id"].as<std::string>());
            point.device_id = StringToUUID(row["device_id"].as<std::string>());
            point.name = row["name"].as<std::string>();
            point.description = row["description"].as<std::string>();
            point.address = row["address"].as<std::string>();
            point.data_type = row["data_type"].as<std::string>();
            point.scaling_factor = row["scaling_factor"].as<double>();
            point.scaling_offset = row["scaling_offset"].as<double>();
            point.min_value = row["min_value"].as<double>();
            point.max_value = row["max_value"].as<double>();
            point.unit = row["unit"].as<std::string>();
            point.is_enabled = row["is_enabled"].as<bool>();
            point.is_writable = row["is_writable"].as<bool>();
            point.scan_rate_ms = row["scan_rate_ms"].as<int>();
            point.deadband = row["deadband"].as<double>();
            point.log_enabled = row["log_enabled"].as<bool>();
            point.log_interval_ms = row["log_interval_ms"].as<int>();
            point.log_deadband = row["log_deadband"].as<double>();
            
            if (!row["tags"].is_null()) {
                point.tags = JsonArrayToStringVector(json::parse(row["tags"].as<std::string>()));
            }
            
            if (!row["metadata"].is_null()) {
                point.metadata = json::parse(row["metadata"].as<std::string>());
            }
            
            point.created_at = StringToTimestamp(row["created_at"].as<std::string>());
            point.updated_at = StringToTimestamp(row["updated_at"].as<std::string>());
            
            datapoints.push_back(point);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("GetDataPoints failed: " + std::string(e.what()));
    }
    
    return datapoints;
}

std::optional<DataPointInfo> DeviceDataAccess::GetDataPoint(const UUID& point_id) {
    try {
        std::string query = "SELECT * FROM " + GetTableName("data_points") + 
                           " WHERE point_id = '" + UUIDToString(point_id) + "'";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        if (result.empty()) {
            return std::nullopt;
        }
        
        const auto& row = result[0];
        DataPointInfo point;
        
        point.point_id = StringToUUID(row["point_id"].as<std::string>());
        point.device_id = StringToUUID(row["device_id"].as<std::string>());
        point.name = row["name"].as<std::string>();
        point.description = row["description"].as<std::string>();
        point.address = row["address"].as<std::string>();
        point.data_type = row["data_type"].as<std::string>();
        point.scaling_factor = row["scaling_factor"].as<double>();
        point.scaling_offset = row["scaling_offset"].as<double>();
        point.min_value = row["min_value"].as<double>();
        point.max_value = row["max_value"].as<double>();
        point.unit = row["unit"].as<std::string>();
        point.is_enabled = row["is_enabled"].as<bool>();
        point.is_writable = row["is_writable"].as<bool>();
        point.scan_rate_ms = row["scan_rate_ms"].as<int>();
        point.deadband = row["deadband"].as<double>();
        point.log_enabled = row["log_enabled"].as<bool>();
        point.log_interval_ms = row["log_interval_ms"].as<int>();
        point.log_deadband = row["log_deadband"].as<double>();
        
        if (!row["tags"].is_null()) {
            point.tags = JsonArrayToStringVector(json::parse(row["tags"].as<std::string>()));
        }
        
        if (!row["metadata"].is_null()) {
            point.metadata = json::parse(row["metadata"].as<std::string>());
        }
        
        point.created_at = StringToTimestamp(row["created_at"].as<std::string>());
        point.updated_at = StringToTimestamp(row["updated_at"].as<std::string>());
        
        return point;
        
    } catch (const std::exception& e) {
        logger_.Error("GetDataPoint failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DeviceDataAccess::CreateDataPoint(const DataPointInfo& point) {
    try {
        std::ostringstream sql;
        sql << "INSERT INTO " << GetTableName("data_points") << " ("
            << "point_id, device_id, name, description, address, data_type, "
            << "scaling_factor, scaling_offset, min_value, max_value, unit, "
            << "is_enabled, is_writable, scan_rate_ms, deadband, "
            << "log_enabled, log_interval_ms, log_deadband, "
            << "tags, metadata, created_at, updated_at"
            << ") VALUES (";
        
        sql << "'" << UUIDToString(point.point_id) << "', ";
        sql << "'" << UUIDToString(point.device_id) << "', ";
        sql << "'" << EscapeString(point.name) << "', ";
        sql << "'" << EscapeString(point.description) << "', ";
        sql << "'" << EscapeString(point.address) << "', ";
        sql << "'" << EscapeString(point.data_type) << "', ";
        sql << point.scaling_factor << ", ";
        sql << point.scaling_offset << ", ";
        sql << point.min_value << ", ";
        sql << point.max_value << ", ";
        sql << "'" << EscapeString(point.unit) << "', ";
        sql << (point.is_enabled ? "true" : "false") << ", ";
        sql << (point.is_writable ? "true" : "false") << ", ";
        sql << point.scan_rate_ms << ", ";
        sql << point.deadband << ", ";
        sql << (point.log_enabled ? "true" : "false") << ", ";
        sql << point.log_interval_ms << ", ";
        sql << point.log_deadband << ", ";
        sql << "'" << EscapeString(StringVectorToJsonArray(point.tags).dump()) << "', ";
        sql << "'" << EscapeString(point.metadata.dump()) << "', ";
        sql << "NOW(), NOW())";
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Info("Created data point: " + point.name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("CreateDataPoint failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::UpdateDataPoint(const DataPointInfo& point) {
    try {
        std::ostringstream sql;
        sql << "UPDATE " << GetTableName("data_points") << " SET "
            << "device_id = '" << UUIDToString(point.device_id) << "', "
            << "name = '" << EscapeString(point.name) << "', "
            << "description = '" << EscapeString(point.description) << "', "
            << "address = '" << EscapeString(point.address) << "', "
            << "data_type = '" << EscapeString(point.data_type) << "', "
            << "scaling_factor = " << point.scaling_factor << ", "
            << "scaling_offset = " << point.scaling_offset << ", "
            << "min_value = " << point.min_value << ", "
            << "max_value = " << point.max_value << ", "
            << "unit = '" << EscapeString(point.unit) << "', "
            << "is_enabled = " << (point.is_enabled ? "true" : "false") << ", "
            << "is_writable = " << (point.is_writable ? "true" : "false") << ", "
            << "scan_rate_ms = " << point.scan_rate_ms << ", "
            << "deadband = " << point.deadband << ", "
            << "log_enabled = " << (point.log_enabled ? "true" : "false") << ", "
            << "log_interval_ms = " << point.log_interval_ms << ", "
            << "log_deadband = " << point.log_deadband << ", "
            << "tags = '" << EscapeString(StringVectorToJsonArray(point.tags).dump()) << "', "
            << "metadata = '" << EscapeString(point.metadata.dump()) << "', "
            << "updated_at = NOW() "
            << "WHERE point_id = '" << UUIDToString(point.point_id) << "'";
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Info("Updated data point: " + point.name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("UpdateDataPoint failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::DeleteDataPoint(const UUID& point_id) {
    try {
        std::string sql = "DELETE FROM " + GetTableName("data_points") + 
                         " WHERE point_id = '" + UUIDToString(point_id) + "'";
        
        bool success = db_manager_->executeNonQueryPostgres(sql);
        
        if (success) {
            logger_.Info("Deleted data point: " + UUIDToString(point_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DeleteDataPoint failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 현재 값 관리
// =============================================================================

std::optional<CurrentValueInfo> DeviceDataAccess::GetCurrentValue(const UUID& point_id) {
    try {
        std::string query = "SELECT * FROM " + GetTableName("current_values") + 
                           " WHERE point_id = '" + UUIDToString(point_id) + "'";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        if (result.empty()) {
            return std::nullopt;
        }
        
        const auto& row = result[0];
        CurrentValueInfo value;
        
        value.point_id = StringToUUID(row["point_id"].as<std::string>());
        value.value = row["value"].as<double>();
        value.quality = row["quality"].as<std::string>();
        value.timestamp = StringToTimestamp(row["timestamp"].as<std::string>());
        
        return value;
        
    } catch (const std::exception& e) {
        logger_.Error("GetCurrentValue failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<CurrentValueInfo> DeviceDataAccess::GetCurrentValues(const std::vector<UUID>& point_ids) {
    std::vector<CurrentValueInfo> values;
    
    if (point_ids.empty()) {
        return values;
    }
    
    try {
        std::string query = "SELECT * FROM " + GetTableName("current_values") + " WHERE point_id IN (";
        
        bool first = true;
        for (const auto& id : point_ids) {
            if (!first) query += ", ";
            first = false;
            query += "'" + UUIDToString(id) + "'";
        }
        query += ")";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        for (const auto& row : result) {
            CurrentValueInfo value;
            
            value.point_id = StringToUUID(row["point_id"].as<std::string>());
            value.value = row["value"].as<double>();
            value.quality = row["quality"].as<std::string>();
            value.timestamp = StringToTimestamp(row["timestamp"].as<std::string>());
            
            values.push_back(value);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("GetCurrentValues failed: " + std::string(e.what()));
    }
    
    return values;
}

bool DeviceDataAccess::UpdateCurrentValue(const UUID& point_id, double value, 
                                          const std::string& quality, const Timestamp& timestamp) {
    try {
        std::ostringstream sql;
        sql << "INSERT INTO " << GetTableName("current_values") << " ("
            << "point_id, value, quality, timestamp"
            << ") VALUES ("
            << "'" << UUIDToString(point_id) << "', "
            << value << ", "
            << "'" << EscapeString(quality) << "', "
            << "'" << TimestampToString(timestamp) << "'"
            << ") ON CONFLICT (point_id) DO UPDATE SET "
            << "value = EXCLUDED.value, "
            << "quality = EXCLUDED.quality, "
            << "timestamp = EXCLUDED.timestamp";
        
        return db_manager_->executeNonQueryPostgres(sql.str());
        
    } catch (const std::exception& e) {
        logger_.Error("UpdateCurrentValue failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::UpdateCurrentValues(const std::vector<CurrentValueInfo>& values) {
    if (values.empty()) {
        return true;
    }
    
    try {
        std::ostringstream sql;
        sql << "INSERT INTO " << GetTableName("current_values") << " ("
            << "point_id, value, quality, timestamp"
            << ") VALUES ";
        
        bool first = true;
        for (const auto& val : values) {
            if (!first) sql << ", ";
            first = false;
            
            sql << "("
                << "'" << UUIDToString(val.point_id) << "', "
                << val.value << ", "
                << "'" << EscapeString(val.quality) << "', "
                << "'" << TimestampToString(val.timestamp) << "'"
                << ")";
        }
        
        sql << " ON CONFLICT (point_id) DO UPDATE SET "
            << "value = EXCLUDED.value, "
            << "quality = EXCLUDED.quality, "
            << "timestamp = EXCLUDED.timestamp";
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Debug("Updated " + std::to_string(values.size()) + " current values in batch");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("UpdateCurrentValues failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 알람 관리 (일부만 구현 - 나머지는 유사한 패턴)
// =============================================================================

std::vector<AlarmConfigInfo> DeviceDataAccess::GetAlarmConfigs(const UUID& factory_id, bool enabled_only) {
    std::vector<AlarmConfigInfo> alarms;
    
    try {
        std::string query = "SELECT * FROM " + GetTableName("alarm_configs") + " WHERE 1=1";
        
        if (!factory_id.empty()) {  // UUID는 std::string이므로 empty() 사용
            query += " AND factory_id = '" + UUIDToString(factory_id) + "'";
        }
        
        if (enabled_only) {
            query += " AND enabled = true";
        }
        
        query += " ORDER BY priority DESC, created_at DESC";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        // 결과 처리... (GetDevices와 유사한 패턴)
        
    } catch (const std::exception& e) {
        logger_.Error("GetAlarmConfigs failed: " + std::string(e.what()));
    }
    
    return alarms;
}

std::vector<ActiveAlarmInfo> DeviceDataAccess::GetActiveAlarms(const UUID& factory_id) {
    std::vector<ActiveAlarmInfo> alarms;
    // 구현...
    return alarms;
}

bool DeviceDataAccess::CreateAlarmConfig(const AlarmConfigInfo& alarm_config) {
    // 구현...
    return false;
}

std::optional<UUID> DeviceDataAccess::TriggerAlarm(const UUID& alarm_config_id, double triggered_value, const std::string& message) {
    // 구현...
    return std::nullopt;
}

bool DeviceDataAccess::AcknowledgeAlarm(const UUID& alarm_instance_id, const UUID& acknowledged_by, const std::string& comment) {
    // 구현...
    return false;
}

bool DeviceDataAccess::ClearAlarm(const UUID& alarm_instance_id) {
    // 구현...
    return false;
}

// =============================================================================
// 가상 포인트 관리
// =============================================================================

std::vector<VirtualPointInfo> DeviceDataAccess::GetVirtualPoints(const UUID& factory_id, bool enabled_only) {
    std::vector<VirtualPointInfo> points;
    // 구현...
    return points;
}

bool DeviceDataAccess::CreateVirtualPoint(const VirtualPointInfo& virtual_point) {
    // 구현...
    return false;
}

// =============================================================================
// 히스토리 데이터 관리
// =============================================================================

bool DeviceDataAccess::SaveHistoryData(const UUID& point_id, double value,
                                       const std::string& quality, const Timestamp& timestamp) {
    try {
        std::ostringstream sql;
        sql << "INSERT INTO " << GetTableName("history_data") << " ("
            << "point_id, value, quality, timestamp"
            << ") VALUES ("
            << "'" << UUIDToString(point_id) << "', "
            << value << ", "
            << "'" << EscapeString(quality) << "', "
            << "'" << TimestampToString(timestamp) << "'"
            << ")";
        
        return db_manager_->executeNonQueryPostgres(sql.str());
        
    } catch (const std::exception& e) {
        logger_.Error("SaveHistoryData failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::SaveHistoryDataBatch(const std::vector<DataHistoryInfo>& history_data) {
    if (history_data.empty()) {
        return true;
    }
    
    try {
        // 대량 데이터는 COPY 명령을 사용하는 것이 효율적이지만
        // 여기서는 간단한 INSERT 방식 사용
        std::ostringstream sql;
        sql << "INSERT INTO " << GetTableName("history_data") << " ("
            << "point_id, value, quality, timestamp"
            << ") VALUES ";
        
        bool first = true;
        for (const auto& data : history_data) {
            if (!first) sql << ", ";
            first = false;
            
            sql << "("
                << "'" << UUIDToString(data.point_id) << "', "
                << data.value << ", "
                << "'" << EscapeString(data.quality) << "', "
                << "'" << TimestampToString(data.timestamp) << "'"
                << ")";
        }
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Debug("Saved " + std::to_string(history_data.size()) + " history records in batch");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("SaveHistoryDataBatch failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<DataHistoryInfo> DeviceDataAccess::GetHistoryData(const UUID& point_id,
                                                               const Timestamp& start_time,
                                                               const Timestamp& end_time,
                                                               int limit) {
    std::vector<DataHistoryInfo> history;
    
    try {
        std::ostringstream query;
        query << "SELECT * FROM " << GetTableName("history_data")
              << " WHERE point_id = '" << UUIDToString(point_id) << "'"
              << " AND timestamp >= '" << TimestampToString(start_time) << "'"
              << " AND timestamp <= '" << TimestampToString(end_time) << "'"
              << " ORDER BY timestamp DESC"
              << " LIMIT " << limit;
        
        auto result = db_manager_->executeQueryPostgres(query.str());
        
        for (const auto& row : result) {
            DataHistoryInfo data;
            
            data.point_id = StringToUUID(row["point_id"].as<std::string>());
            data.value = row["value"].as<double>();
            data.quality = row["quality"].as<std::string>();
            data.timestamp = StringToTimestamp(row["timestamp"].as<std::string>());
            
            history.push_back(data);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("GetHistoryData failed: " + std::string(e.what()));
    }
    
    return history;
}

// =============================================================================
// 통계 및 집계
// =============================================================================

json DeviceDataAccess::GetDeviceStatistics(const UUID& device_id) {
    json stats;
    
    try {
        // 디바이스 통계 쿼리
        std::string query = "SELECT "
                           "COUNT(*) as total_points, "
                           "SUM(CASE WHEN is_enabled THEN 1 ELSE 0 END) as enabled_points "
                           "FROM " + GetTableName("data_points") + 
                           " WHERE device_id = '" + UUIDToString(device_id) + "'";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        if (!result.empty()) {
            const auto& row = result[0];
            stats["total_points"] = row["total_points"].as<int>();
            stats["enabled_points"] = row["enabled_points"].as<int>();
        }
        
        // 추가 통계...
        
    } catch (const std::exception& e) {
        logger_.Error("GetDeviceStatistics failed: " + std::string(e.what()));
    }
    
    return stats;
}

json DeviceDataAccess::GetAlarmStatistics(const UUID& factory_id) {
    json stats;
    
    try {
        // 알람 통계 쿼리
        std::string query = "SELECT "
                           "priority, COUNT(*) as count "
                           "FROM " + GetTableName("active_alarms") + 
                           " WHERE factory_id = '" + UUIDToString(factory_id) + "'"
                           " AND is_active = true"
                           " GROUP BY priority";
        
        auto result = db_manager_->executeQueryPostgres(query);
        
        json priority_counts;
        for (const auto& row : result) {
            priority_counts[row["priority"].as<std::string>()] = row["count"].as<int>();
        }
        stats["priority_counts"] = priority_counts;
        
        // 추가 통계...
        
    } catch (const std::exception& e) {
        logger_.Error("GetAlarmStatistics failed: " + std::string(e.what()));
    }
    
    return stats;
}

// =============================================================================
// 트랜잭션 지원
// =============================================================================

bool DeviceDataAccess::BeginTransaction() {
    try {
        return db_manager_->executeNonQueryPostgres("BEGIN");
    } catch (const std::exception& e) {
        logger_.Error("BeginTransaction failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::CommitTransaction() {
    try {
        return db_manager_->executeNonQueryPostgres("COMMIT");
    } catch (const std::exception& e) {
        logger_.Error("CommitTransaction failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceDataAccess::RollbackTransaction() {
    try {
        return db_manager_->executeNonQueryPostgres("ROLLBACK");
    } catch (const std::exception& e) {
        logger_.Error("RollbackTransaction failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// ID 변환 유틸리티 메소드
// =============================================================================

std::string DeviceDataAccess::IntToUUID(int id) {
    // 간단한 변환: int를 UUID 형식으로
    // 실제로는 더 복잡한 매핑이 필요할 수 있음
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(8) << std::hex << id;
    oss << "-0000-0000-0000-000000000000";
    return oss.str();
}

int DeviceDataAccess::UUIDToInt(const UUID& uuid) {
    try {
        // UUID의 첫 8자리를 int로 변환
        std::string hex_part = uuid.substr(0, 8);
        return std::stoi(hex_part, nullptr, 16);
    } catch (...) {
        return -1;
    }
}

// =============================================================================
// 디바이스 관련 오버로드 메소드
// =============================================================================

bool DeviceDataAccess::GetDevice(int device_id, DeviceInfo& device_info) {
    // int ID를 UUID로 변환하여 기존 메소드 호출
    UUID uuid = IntToUUID(device_id);
    auto result = GetDevice(uuid);
    
    if (result.has_value()) {
        device_info = result.value();
        return true;
    }
    return false;
}

// =============================================================================
// 데이터 포인트 관련
// =============================================================================

std::vector<DataPointInfo> DeviceDataAccess::GetEnabledDataPoints(int device_id) {
    // int ID를 UUID로 변환하여 기존 메소드 호출
    UUID uuid = IntToUUID(device_id);
    return GetDataPoints(uuid, true); // enabled_only = true
}

// =============================================================================
// 디바이스 상태 관리
// =============================================================================

bool DeviceDataAccess::UpdateDeviceStatus(const DeviceStatus& status) {
    try {
        std::ostringstream sql;
        sql << "INSERT INTO " << GetTableName("device_status") << " ("
            << "device_id, connection_status, last_communication, "
            << "error_count, last_error, response_time, firmware_version, "
            << "hardware_info, diagnostic_data, updated_at"
            << ") VALUES (";
        
        sql << "'" << IntToUUID(status.device_id) << "', ";
        sql << "'" << EscapeString(status.connection_status) << "', ";
        sql << "NOW(), ";
        sql << status.error_count << ", ";
        sql << "'" << EscapeString(status.last_error) << "', ";
        sql << status.response_time << ", ";
        sql << "'" << EscapeString(status.firmware_version) << "', ";
        sql << "'" << EscapeString(status.hardware_info) << "', ";
        sql << "'" << EscapeString(status.diagnostic_data) << "', ";
        sql << "NOW()";
        sql << ") ON CONFLICT (device_id) DO UPDATE SET ";
        sql << "connection_status = EXCLUDED.connection_status, ";
        sql << "last_communication = EXCLUDED.last_communication, ";
        sql << "error_count = EXCLUDED.error_count, ";
        sql << "last_error = EXCLUDED.last_error, ";
        sql << "response_time = EXCLUDED.response_time, ";
        sql << "firmware_version = EXCLUDED.firmware_version, ";
        sql << "hardware_info = EXCLUDED.hardware_info, ";
        sql << "diagnostic_data = EXCLUDED.diagnostic_data, ";
        sql << "updated_at = EXCLUDED.updated_at";
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Debug("Updated device status for device: " + std::to_string(status.device_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("UpdateDeviceStatus failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 현재 값 관리 오버로드
// =============================================================================

bool DeviceDataAccess::UpdateCurrentValue(int point_id, const CurrentValue& current_value) {
    // int ID를 UUID로 변환하여 기존 메소드 호출
    UUID uuid = IntToUUID(point_id);
    return UpdateCurrentValue(uuid, current_value.value, current_value.quality, current_value.timestamp);
}

bool DeviceDataAccess::UpdateCurrentValuesBatch(const std::map<int, CurrentValue>& values) {
    if (values.empty()) {
        return true;
    }
    
    try {
        std::ostringstream sql;
        sql << "INSERT INTO " << GetTableName("current_values") << " ("
            << "point_id, value, quality, timestamp"
            << ") VALUES ";
        
        bool first = true;
        for (const auto& [point_id, val] : values) {
            if (!first) sql << ", ";
            first = false;
            
            sql << "("
                << "'" << IntToUUID(point_id) << "', "
                << val.value << ", "
                << "'" << EscapeString(val.quality) << "', "
                << "'" << TimestampToString(val.timestamp) << "'"
                << ")";
        }
        
        sql << " ON CONFLICT (point_id) DO UPDATE SET "
            << "value = EXCLUDED.value, "
            << "quality = EXCLUDED.quality, "
            << "timestamp = EXCLUDED.timestamp";
        
        bool success = db_manager_->executeNonQueryPostgres(sql.str());
        
        if (success) {
            logger_.Debug("Updated " + std::to_string(values.size()) + " current values in batch");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("UpdateCurrentValuesBatch failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 팩토리 패턴 구현 - IDataAccess 인터페이스 래퍼
// =============================================================================

/**
 * @brief DeviceDataAccess를 IDataAccess 인터페이스로 래핑하는 어댑터 클래스
 * 
 * 이 클래스는 팩토리 패턴을 위해 DeviceDataAccess를 IDataAccess 인터페이스로 
 * 적응시킵니다. 실제 구현은 DeviceDataAccess에 위임합니다.
 */
class DeviceDataAccessAdapter : public IDataAccess {
public:
    DeviceDataAccessAdapter(std::shared_ptr<LogManager> logger,
                           std::shared_ptr<ConfigManager> config)
        : IDataAccess(logger, config) {
        
        // 설정에서 테넌트 코드 읽기
        tenant_code_ = config->getOrDefault("tenant_code", "default");
    }
    
    ~DeviceDataAccessAdapter() override = default;
    
    // IDataAccess 인터페이스 구현
    std::string GetDomainName() const override {
        return "DeviceDataAccess";
    }
    
    static std::string GetStaticDomainName() {
        return "DeviceDataAccess";
    }
    
    bool Initialize() override {
        try {
            // DatabaseManager 싱글턴 가져오기
            auto& db_manager_singleton = DatabaseManager::getInstance();
            
            // PostgreSQL 연결 확인
            if (!db_manager_singleton.isPostgresConnected()) {
                SetError("PostgreSQL connection not available");
                return false;
            }
            
            // shared_ptr로 래핑하기 위해 커스텀 deleter 사용
            // 싱글턴이므로 실제로 삭제하지 않음
            auto db_manager = std::shared_ptr<DatabaseManager>(
                &db_manager_singleton,
                [](DatabaseManager*) {} // no-op deleter
            );
            
            // DeviceDataAccess 인스턴스 생성
            device_data_access_ = std::make_unique<DeviceDataAccess>(
                db_manager,
                tenant_code_
            );
            
            logger_->Info("DeviceDataAccessAdapter initialized successfully");
            return true;
            
        } catch (const std::exception& e) {
            SetError("Initialization failed: " + std::string(e.what()));
            return false;
        }
    }
    
    void Cleanup() override {
        device_data_access_.reset();
        logger_->Info("DeviceDataAccessAdapter cleaned up");
    }
    
    bool IsConnected() const override {
        return device_data_access_ != nullptr && 
               DatabaseManager::getInstance().isPostgresConnected();
    }
    
    bool HealthCheck() override {
        if (!IsConnected()) {
            return false;
        }
        
        try {
            // 간단한 쿼리로 연결 테스트
            auto factories = device_data_access_->GetFactories(true);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    std::unique_ptr<TransactionScope> BeginTransaction() override {
        // 간단한 트랜잭션 스코프 구현
        class SimpleTransactionScope : public TransactionScope {
        public:
            SimpleTransactionScope(DeviceDataAccess* dda) : dda_(dda), active_(true) {
                success_ = dda_->BeginTransaction();
            }
            
            ~SimpleTransactionScope() {
                if (active_ && !committed_) {
                    dda_->RollbackTransaction();
                }
            }
            
            bool Commit() override {
                if (!active_ || committed_) return false;
                committed_ = true;
                return dda_->CommitTransaction();
            }
            
            bool Rollback() override {
                if (!active_ || committed_) return false;
                active_ = false;
                return dda_->RollbackTransaction();
            }
            
            bool IsActive() const override {
                return active_ && !committed_;
            }
            
        private:
            DeviceDataAccess* dda_;
            bool active_;
            bool committed_ = false;
            bool success_;
        };
        
        return std::make_unique<SimpleTransactionScope>(device_data_access_.get());
    }
    
    // DeviceDataAccess 인스턴스에 직접 접근
    DeviceDataAccess* GetDeviceDataAccess() {
        return device_data_access_.get();
    }
    
private:
    std::string tenant_code_;
    std::unique_ptr<DeviceDataAccess> device_data_access_;
};

// =============================================================================
// 팩토리 구현
// =============================================================================

class DeviceDataAccessFactory : public IDataAccessFactory {
public:
    std::unique_ptr<IDataAccess> Create(std::shared_ptr<LogManager> logger,
                                       std::shared_ptr<ConfigManager> config) override {
        return std::make_unique<DeviceDataAccessAdapter>(logger, config);
    }
    
    std::string GetDomainName() const override {
        return "DeviceDataAccess";
    }
};

} // namespace Database
} // namespace PulseOne

// =============================================================================
// 팩토리 자동 등록
// =============================================================================
namespace {
    static bool registered_DeviceDataAccessFactory = 
        PulseOne::Database::DataAccessManager::RegisterFactory(
            "DeviceDataAccess", 
            std::make_unique<PulseOne::Database::DeviceDataAccessFactory>()
        );
}