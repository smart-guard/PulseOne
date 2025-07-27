// =============================================================================
// collector/src/Database/Entities/DeviceEntity.cpp
// PulseOne 디바이스 엔티티 구현 - 기존 패턴 100% 준수
// =============================================================================

#include "Database/Entities/DeviceEntity.h"
#include "Common/Constants.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace PulseOne::Constants;

namespace PulseOne {
namespace Database {
namespace Entities { 
// =============================================================================
// 생성자 및 소멸자
// =============================================================================

DeviceEntity::DeviceEntity() 
    : BaseEntity<DeviceEntity>()
    , tenant_id_(0)
    , site_id_(0)
    , data_points_loaded_(false)
    , alarm_configs_loaded_(false) {
    
    // DeviceInfo 기본값 설정
    device_info_.is_enabled = true;
    device_info_.status = "disconnected";
    device_info_.last_seen = std::chrono::system_clock::now();
    device_info_.polling_interval_ms = 1000;
    device_info_.timeout_ms = 5000;
    device_info_.retry_count = 3;
}

DeviceEntity::DeviceEntity(int device_id) 
    : BaseEntity<DeviceEntity>(device_id)
    , tenant_id_(0)
    , site_id_(0)
    , data_points_loaded_(false)
    , alarm_configs_loaded_(false) {
    
    device_info_.is_enabled = true;
    device_info_.status = "disconnected";
    device_info_.last_seen = std::chrono::system_clock::now();
    device_info_.polling_interval_ms = 1000;
    device_info_.timeout_ms = 5000;
    device_info_.retry_count = 3;
}

DeviceEntity::DeviceEntity(const DeviceInfo& device_info) 
    : BaseEntity<DeviceEntity>()
    , device_info_(device_info)
    , tenant_id_(0)
    , site_id_(0)
    , data_points_loaded_(false)
    , alarm_configs_loaded_(false) {
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현
// =============================================================================

bool DeviceEntity::loadFromDatabase() {
    if (id_ <= 0) {
        logger_.Error("DeviceEntity::loadFromDatabase - Invalid device ID: " + std::to_string(id_));
        markError();
        return false;
    }
    
    try {
        std::string query = "SELECT * FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        auto results = executeUnifiedQuery(query);
        
        if (results.empty()) {
            logger_.Warn("DeviceEntity::loadFromDatabase - Device not found: " + std::to_string(id_));
            return false;
        }
        
        // 첫 번째 행을 DeviceInfo로 변환
        device_info_ = mapRowToDeviceInfo(results[0]);
        
        // 추가 필드들 매핑
        auto it = results[0].find("tenant_id");
        if (it != results[0].end()) {
            tenant_id_ = std::stoi(it->second);
        }
        
        it = results[0].find("site_id");
        if (it != results[0].end()) {
            site_id_ = std::stoi(it->second);
        }
        
        markSaved();
        logger_.Info("DeviceEntity::loadFromDatabase - Loaded device: " + device_info_.name);
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool DeviceEntity::saveToDatabase() {
    if (!isValid()) {
        logger_.Error("DeviceEntity::saveToDatabase - Invalid device data");
        return false;
    }
    
    try {
        std::string sql = buildInsertSQL();
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            // SQLite인 경우 마지막 INSERT ID 조회
            std::string db_type = config_manager_.getOrDefault("DATABASE_TYPE", "SQLITE");
            if (db_type == "SQLITE") {
                auto results = executeUnifiedQuery("SELECT last_insert_rowid() as id");
                if (!results.empty()) {
                    id_ = std::stoi(results[0]["id"]);
                }
            }
            
            markSaved();
            logger_.Info("DeviceEntity::saveToDatabase - Saved device: " + device_info_.name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool DeviceEntity::updateToDatabase() {
    if (id_ <= 0 || !isValid()) {
        logger_.Error("DeviceEntity::updateToDatabase - Invalid device data or ID");
        return false;
    }
    
    try {
        std::string sql = buildUpdateSQL();
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markSaved();
            logger_.Info("DeviceEntity::updateToDatabase - Updated device: " + device_info_.name);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::updateToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool DeviceEntity::deleteFromDatabase() {
    if (id_ <= 0) {
        logger_.Error("DeviceEntity::deleteFromDatabase - Invalid device ID");
        return false;
    }
    
    try {
        std::string sql = "DELETE FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markDeleted();
            logger_.Info("DeviceEntity::deleteFromDatabase - Deleted device ID: " + std::to_string(id_));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::deleteFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

// =============================================================================
// JSON 직렬화
// =============================================================================

json DeviceEntity::toJson() const {
    json j;
    
    try {
        // 기본 정보
        j["id"] = id_;
        j["tenant_id"] = tenant_id_;
        j["site_id"] = site_id_;
        
        // DeviceInfo 직렬화
        j["name"] = device_info_.name;
        j["description"] = device_info_.description;
        j["protocol_type"] = device_info_.protocol_type;
        j["connection_string"] = device_info_.connection_string;
        j["is_enabled"] = device_info_.is_enabled;
        j["status"] = device_info_.status;
        j["polling_interval_ms"] = device_info_.polling_interval_ms;
        j["timeout_ms"] = device_info_.timeout_ms;
        j["retry_count"] = device_info_.retry_count;
        
        // 설정 (이미 JSON이면 그대로, 아니면 파싱)
        j["config"] = device_info_.connection_config.empty() ? 
              json::object() : 
              device_info_.connection_config;   
        
        // 태그
        j["tags"] = device_info_.tags;
        
        // 메타데이터
        if (!device_info_.metadata.empty()) {
            j["metadata"] = device_info_.metadata;
        } else {
            j["metadata"] = json::object();
        }
        
        // 타임스탬프
        j["created_at"] = timestampToString(created_at_);
        j["updated_at"] = timestampToString(updated_at_);
        j["last_seen"] = timestampToString(device_info_.last_seen);
        
        // 상태 정보
        j["state"] = static_cast<int>(state_);
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::toJson failed: " + std::string(e.what()));
    }
    
    return j;
}

bool DeviceEntity::fromJson(const json& data) {
    try {
        // 기본 정보
        if (data.contains("id")) {
            id_ = data["id"].get<int>();
        }
        if (data.contains("tenant_id")) {
            tenant_id_ = data["tenant_id"].get<int>();
        }
        if (data.contains("site_id")) {
            site_id_ = data["site_id"].get<int>();
        }
        
        // DeviceInfo 역직렬화
        if (data.contains("name")) {
            device_info_.name = data["name"].get<std::string>();
        }
        if (data.contains("description")) {
            device_info_.description = data["description"].get<std::string>();
        }
        if (data.contains("protocol_type")) {
            device_info_.protocol_type = data["protocol_type"].get<std::string>();
        }
        if (data.contains("connection_string")) {
            device_info_.connection_string = data["connection_string"].get<std::string>();
        }
        if (data.contains("is_enabled")) {
            device_info_.is_enabled = data["is_enabled"].get<bool>();
        }
        if (data.contains("status")) {
            device_info_.status = data["status"].get<std::string>();
        }
        if (data.contains("polling_interval_ms")) {
            device_info_.polling_interval_ms = data["polling_interval_ms"].get<int>();
        }
        if (data.contains("timeout_ms")) {
            device_info_.timeout_ms = data["timeout_ms"].get<int>();
        }
        if (data.contains("retry_count")) {
            device_info_.retry_count = data["retry_count"].get<int>();
        }
        
        // 설정 JSON
        if (data.contains("config")) {
            device_info_.connection_config = data["config"].dump();
        }
        
        // 태그
        if (data.contains("tags")) {
            device_info_.tags = data["tags"].get<std::vector<std::string>>();
        }
        
        // 메타데이터
        if (data.contains("metadata")) {
            device_info_.metadata = data["metadata"];
        }
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::fromJson failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

std::string DeviceEntity::toString() const {
    std::ostringstream oss;
    oss << "DeviceEntity[";
    oss << "id=" << id_;
    oss << ", name=" << device_info_.name;
    oss << ", protocol=" << device_info_.protocol_type;
    oss << ", endpoint=" << device_info_.connection_string;
    oss << ", enabled=" << (device_info_.is_enabled ? "true" : "false");
    oss << ", status=" << device_info_.status;
    oss << ", state=" << static_cast<int>(state_);
    oss << "]";
    return oss.str();
}

// =============================================================================
// 프로토콜별 설정 추출
// =============================================================================

json DeviceEntity::extractModbusConfig() const {
    return parseProtocolConfig("modbus", device_info_.connection_config);
}

json DeviceEntity::extractMqttConfig() const {
    return parseProtocolConfig("mqtt", device_info_.connection_config);
}

json DeviceEntity::extractBacnetConfig() const {
    return parseProtocolConfig("bacnet", device_info_.connection_config);
}

json DeviceEntity::extractProtocolConfig() const {
    std::string protocol_lower = device_info_.protocol_type;
    std::transform(protocol_lower.begin(), protocol_lower.end(), protocol_lower.begin(), ::tolower);
    
    if (protocol_lower.find("modbus") != std::string::npos) {
        return extractModbusConfig();
    } else if (protocol_lower.find("mqtt") != std::string::npos) {
        return extractMqttConfig();
    } else if (protocol_lower.find("bacnet") != std::string::npos) {
        return extractBacnetConfig();
    } else {
        // 범용 설정 반환
        try {
            if (!device_info_.connection_config.empty()) {
                return json::parse(device_info_.connection_config);
            }
        } catch (const std::exception& e) {
            logger_.Warn("DeviceEntity::extractProtocolConfig - Failed to parse config: " + std::string(e.what()));
        }
        return json::object();
    }
}

// =============================================================================
// 실시간 데이터 RDB 저장 지원
// =============================================================================

json DeviceEntity::getRDBContext() const {
    json context;
    
    context["device_id"] = std::to_string(id_);  // int를 string으로 변환
    context["device_name"] = device_info_.name;
    context["protocol_type"] = device_info_.protocol_type;
    context["tenant_id"] = tenant_id_;
    context["site_id"] = site_id_;
    
    // 메타데이터 추가
    context["metadata"]["endpoint"] = device_info_.connection_string;
    context["metadata"]["status"] = device_info_.status;
    context["metadata"]["polling_interval"] = std::to_string(device_info_.polling_interval_ms);
    
    return context;
}

int DeviceEntity::saveRealtimeDataToRDB(const std::vector<json>& values) {
    if (values.empty()) {
        return 0;
    }
    
    try {
        int saved_count = 0;
        
        for (const auto& value : values) {
            std::ostringstream sql;
            sql << "INSERT INTO data_history (point_id, device_id, value, quality, timestamp) VALUES (";
            sql << "'" << escapeString(value["point_id"].get<std::string>()) << "', ";
            sql << id_ << ", ";
            sql << value["value"].get<double>() << ", ";
            sql << "'" << escapeString(value["quality"].get<std::string>()) << "', ";
            sql << "'" << timestampToString(std::chrono::system_clock::now()) << "'";
            sql << ")";
            
            if (executeUnifiedNonQuery(sql.str())) {
                saved_count++;
            }
        }
        
        logger_.Info("DeviceEntity::saveRealtimeDataToRDB - Saved " + std::to_string(saved_count) + " values");
        return saved_count;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::saveRealtimeDataToRDB failed: " + std::string(e.what()));
        return 0;
    }
}

/*
int DeviceEntity::saveRealtimeDataToRDB(const std::vector<RealtimeValueEntity>& values) {
    if (values.empty()) {
        return 0;
    }
    
    try {
        int saved_count = 0;
        
        for (const auto& value : values) {
            std::ostringstream sql;
            sql << "INSERT INTO data_history (point_id, device_id, value, quality, timestamp) VALUES (";
            sql << "'" << escapeString(value.point_id) << "', ";
            sql << id_ << ", ";
            sql << value.value << ", ";
            sql << "'" << escapeString(value.quality) << "', ";
            sql << "'" << timestampToString(value.timestamp) << "'";
            sql << ")";
            
            if (executeUnifiedNonQuery(sql.str())) {
                saved_count++;
            }
        }
        
        logger_.Info("DeviceEntity::saveRealtimeDataToRDB - Saved " + std::to_string(saved_count) + " values");
        return saved_count;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::saveRealtimeDataToRDB failed: " + std::string(e.what()));
        return 0;
    }
}
*/

// =============================================================================
// 관계 엔티티 로드 (lazy loading)
// =============================================================================

std::vector<DataPoint> DeviceEntity::getDataPoints(bool enabled_only) {
    if (data_points_loaded_ && cached_data_points_.has_value()) {
        return cached_data_points_.value();
    }
    
    try {
        std::string query = "SELECT * FROM data_points WHERE device_id = " + std::to_string(id_);
        
        if (enabled_only) {
            query += " AND is_enabled = 1";  // SQLite/PostgreSQL 호환
        }
        
        query += " ORDER BY name";
        
        auto results = executeUnifiedQuery(query);
        
        std::vector<DataPoint> data_points;
        
        for (const auto& row : results) {
            DataPoint point;
            
            // 기본 필드 매핑 (UnifiedCommonTypes.h의 DataPoint 구조 활용)
            auto it = row.find("id");
            if (it != row.end()) {
                point.id = it->second;
            }
            
            point.device_id = std::to_string(id_);
            
            it = row.find("name");
            if (it != row.end()) {
                point.name = it->second;
            }
            
            it = row.find("description");
            if (it != row.end()) {
                point.description = it->second;
            }
            
            it = row.find("address");
            if (it != row.end()) {
                point.address = std::stoi(it->second);
                point.address_string = it->second;
            }
            
            it = row.find("data_type");
            if (it != row.end()) {
                point.data_type = it->second;
            }
            
            it = row.find("is_enabled");
            if (it != row.end()) {
                point.is_enabled = (it->second == "1" || it->second == "true");
            }
            
            data_points.push_back(point);
        }
        
        // 캐시에 저장
        cached_data_points_ = data_points;
        data_points_loaded_ = true;
        
        logger_.Info("DeviceEntity::getDataPoints - Loaded " + std::to_string(data_points.size()) + " points");
        return data_points;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::getDataPoints failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<json> DeviceEntity::getAlarmConfigs() {
    if (alarm_configs_loaded_ && cached_alarm_configs_.has_value()) {
        return cached_alarm_configs_.value();
    }
    
    try {
        std::string query = "SELECT * FROM alarm_definitions WHERE device_id = " + std::to_string(id_);
        query += " ORDER BY alarm_name";
        
        auto results = executeUnifiedQuery(query);
        
        std::vector<json> alarm_configs;
        
        for (const auto& row : results) {
            json alarm;
            
            for (const auto& field : row) {
                alarm[field.first] = field.second;
            }
            
            alarm_configs.push_back(alarm);
        }
        
        // 캐시에 저장
        cached_alarm_configs_ = alarm_configs;
        alarm_configs_loaded_ = true;
        
        logger_.Info("DeviceEntity::getAlarmConfigs - Loaded " + std::to_string(alarm_configs.size()) + " alarms");
        return alarm_configs;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::getAlarmConfigs failed: " + std::string(e.what()));
        return {};
    }
}

bool DeviceEntity::updateDeviceStatus(const std::string& status, const std::string& error_message) {
    try {
        device_info_.status = status;
        device_info_.last_seen = std::chrono::system_clock::now();
        
        std::ostringstream sql;
        sql << "UPDATE " << getTableName() << " SET ";
        sql << "status = '" << escapeString(status) << "', ";
        sql << "last_seen = '" << timestampToString(device_info_.last_seen) << "'";
        
        if (!error_message.empty()) {
            sql << ", last_error = '" << escapeString(error_message) << "'";
        }
        
        sql << " WHERE id = " << id_;
        
        bool success = executeUnifiedNonQuery(sql.str());
        
        if (success) {
            markModified();
            logger_.Info("DeviceEntity::updateDeviceStatus - Updated status: " + status);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceEntity::updateDeviceStatus failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 유효성 검사
// =============================================================================

bool DeviceEntity::isValid() const {
    if (!BaseEntity<DeviceEntity>::isValid()) {
        return false;
    }
    
    // 필수 필드 검사
    if (device_info_.name.empty()) {
        return false;
    }
    
    if (device_info_.protocol_type.empty()) {
        return false;
    }
    
    if (device_info_.connection_string.empty()) {
        return false;
    }
    
    // 값 범위 검사
    if (device_info_.polling_interval_ms < 0) {
        return false;
    }
    
    if (device_info_.timeout_ms < 0) {
        return false;
    }
    
    if (device_info_.retry_count < 0) {
        return false;
    }
    
    return true;
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

DeviceInfo DeviceEntity::mapRowToDeviceInfo(const std::map<std::string, std::string>& row) {
    DeviceInfo info;
    
    auto getValue = [&row](const std::string& key) -> std::string {
        auto it = row.find(key);
        return (it != row.end()) ? it->second : "";
    };
    
    // 기본 필드들
    info.name = getValue("name");
    info.description = getValue("description");
    info.protocol_type = getValue("protocol_type");
    info.connection_string = getValue("endpoint");  // DB 컬럼명 매핑
    
    // Boolean 필드들
    std::string enabled_str = getValue("is_enabled");
    info.is_enabled = (enabled_str == "1" || enabled_str == "true");
    
    info.status = getValue("status");
    
    // 숫자 필드들
    std::string polling_str = getValue("polling_interval");
    if (!polling_str.empty()) {
        info.polling_interval_ms = std::stoi(polling_str);
    }
    
    std::string timeout_str = getValue("timeout_ms");
    if (!timeout_str.empty()) {
        info.timeout_ms = std::stoi(timeout_str);
    }
    
    std::string retry_str = getValue("retry_count");
    if (!retry_str.empty()) {
        info.retry_count = std::stoi(retry_str);
    }
    
    // JSON 필드들
    info.connection_config = getValue("config");
    
    // 태그 (JSON 배열로 저장된 경우)
    std::string tags_str = getValue("tags");
    if (!tags_str.empty()) {
        try {
            json tags_json = json::parse(tags_str);
            if (tags_json.is_array()) {
                info.tags = tags_json.get<std::vector<std::string>>();
            }
        } catch (const std::exception&) {
            // 파싱 실패 시 무시
        }
    }
    
    // 메타데이터
    std::string metadata_str = getValue("metadata");
    if (!metadata_str.empty()) {
        try {
            info.metadata = json::parse(metadata_str);
        } catch (const std::exception&) {
            // 파싱 실패 시 무시
        }
    }
    
    // 타임스탬프 필드들 (간단한 구현)
    info.created_at = std::chrono::system_clock::now();
    info.updated_at = std::chrono::system_clock::now();
    info.last_seen = std::chrono::system_clock::now();
    
    return info;
}

std::string DeviceEntity::buildInsertSQL() const {
    std::ostringstream sql;
    
    sql << "INSERT INTO " << getTableName() << " (";
    sql << "tenant_id, site_id, name, description, protocol_type, endpoint, config, ";
    sql << "is_enabled, status, polling_interval, timeout_ms, retry_count, ";
    sql << "created_at, updated_at";
    sql << ") VALUES (";
    sql << tenant_id_ << ", ";
    sql << site_id_ << ", ";
    sql << "'" << escapeString(device_info_.name) << "', ";
    sql << "'" << escapeString(device_info_.description) << "', ";
    sql << "'" << escapeString(device_info_.protocol_type) << "', ";
    sql << "'" << escapeString(device_info_.connection_string) << "', ";
    sql << "'" << escapeString(device_info_.connection_config) << "', ";
    sql << (device_info_.is_enabled ? "1" : "0") << ", ";
    sql << "'" << escapeString(device_info_.status) << "', ";
    sql << device_info_.polling_interval_ms << ", ";
    sql << device_info_.timeout_ms << ", ";
    sql << device_info_.retry_count << ", ";
    sql << "'" << timestampToString(created_at_) << "', ";
    sql << "'" << timestampToString(updated_at_) << "'";
    sql << ")";
    
    return sql.str();
}

std::string DeviceEntity::buildUpdateSQL() const {
    std::ostringstream sql;
    
    sql << "UPDATE " << getTableName() << " SET ";
    sql << "tenant_id = " << tenant_id_ << ", ";
    sql << "site_id = " << site_id_ << ", ";
    sql << "name = '" << escapeString(device_info_.name) << "', ";
    sql << "description = '" << escapeString(device_info_.description) << "', ";
    sql << "protocol_type = '" << escapeString(device_info_.protocol_type) << "', ";
    sql << "endpoint = '" << escapeString(device_info_.connection_string) << "', ";
    sql << "config = '" << escapeString(device_info_.connection_config) << "', ";
    sql << "is_enabled = " << (device_info_.is_enabled ? "1" : "0") << ", ";
    sql << "status = '" << escapeString(device_info_.status) << "', ";
    sql << "polling_interval = " << device_info_.polling_interval_ms << ", ";
    sql << "timeout_ms = " << device_info_.timeout_ms << ", ";
    sql << "retry_count = " << device_info_.retry_count << ", ";
    sql << "updated_at = '" << timestampToString(updated_at_) << "' ";
    sql << "WHERE id = " << id_;
    
    return sql.str();
}

json DeviceEntity::parseProtocolConfig(const std::string& protocol_type, const std::string& config_json) const {
    try {
        if (config_json.empty()) {
            return json::object();
        }
        
        json config = json::parse(config_json);
        
        // 프로토콜별 기본값 추가 (필요시)
        std::string protocol_lower = protocol_type;
        std::transform(protocol_lower.begin(), protocol_lower.end(), protocol_lower.begin(), ::tolower);
        
        if (protocol_lower.find("modbus") != std::string::npos) {
            // Modbus 기본값
            if (!config.contains("timeout_ms")) {
                config["timeout_ms"] = device_info_.timeout_ms;
            }
            if (!config.contains("retry_count")) {
                config["retry_count"] = device_info_.retry_count;
            }
        } else if (protocol_lower.find("mqtt") != std::string::npos) {
            // MQTT 기본값
            if (!config.contains("keep_alive")) {
                config["keep_alive"] = 60;
            }
            if (!config.contains("qos")) {
                config["qos"] = 1;
            }
        }
        
        return config;
        
    } catch (const std::exception& e) {
        logger_.Warn("DeviceEntity::parseProtocolConfig failed: " + std::string(e.what()));
        return json::object();
    }
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne