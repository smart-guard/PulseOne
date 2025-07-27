/**
 * @file DataPointEntity.cpp
 * @brief PulseOne 데이터포인트 엔티티 구현 (SQLite 스키마 기반)
 * @author PulseOne Development Team
 * @date 2025-07-27
 */

#include "Database/Entities/DataPointEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <sstream>
#include <iomanip>
#include <limits>

namespace PulseOne {
namespace Database {
namespace Entities {  // 🔥 올바른 네임스페이스!

// =======================================================================
// 생성자 및 소멸자
// =======================================================================

DataPointEntity::DataPointEntity()
    : BaseEntity<DataPointEntity>()
    , device_id_(0)
    , name_("")
    , description_("")
    , address_(0)
    , data_type_("float")
    , access_mode_("read")
    , unit_("")
    , scaling_factor_(1.0)
    , scaling_offset_(0.0)
    , min_value_(std::numeric_limits<double>::lowest())
    , max_value_(std::numeric_limits<double>::max())
    , is_enabled_(true)
    , scan_rate_(std::nullopt)
    , deadband_(0.0)
    , config_(json::object())
    , tags_()
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    , device_info_loaded_(false)
    , alarm_configs_loaded_(false)
    , current_value_loaded_(false) {
}

DataPointEntity::DataPointEntity(int point_id)
    : DataPointEntity() {
    setId(point_id);
}

DataPointEntity::DataPointEntity(int device_id, int address)
    : DataPointEntity() {
    device_id_ = device_id;
    address_ = address;
}

// =======================================================================
// BaseEntity 순수 가상 함수 구현
// =======================================================================

bool DataPointEntity::loadFromDatabase() {
    if (getId() <= 0) {
        getLogger().Error("DataPointEntity::loadFromDatabase - Invalid ID");
        return false;
    }
    
    try {
        std::ostringstream query;
        query << "SELECT * FROM data_points WHERE id = " << getId();
        
        auto& db_manager = getDatabaseManager();
        auto result = db_manager.executeQuerySQLite(query.str());
        
        if (result.empty()) {
            getLogger().Warning("DataPointEntity::loadFromDatabase - Point not found: " + std::to_string(getId()));
            return false;
        }
        
        return mapRowToEntity(result[0]);
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::loadFromDatabase failed: " + std::string(e.what()));
        setState(EntityState::ERROR);
        return false;
    }
}

bool DataPointEntity::saveToDatabase() {
    try {
        std::string sql = buildInsertSQL();
        
        auto& db_manager = getDatabaseManager();
        bool success = db_manager.executeUpdateSQLite(sql);
        
        if (success) {
            // SQLite의 마지막 삽입 ID 조회
            auto result = db_manager.executeQuerySQLite("SELECT last_insert_rowid() as id");
            if (!result.empty()) {
                setId(std::stoi(result[0].at("id")));
            }
            
            setState(EntityState::LOADED);
            getLogger().Info("DataPointEntity saved with ID: " + std::to_string(getId()));
            return true;
        }
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::saveToDatabase failed: " + std::string(e.what()));
        setState(EntityState::ERROR);
    }
    
    return false;
}

bool DataPointEntity::updateToDatabase() {
    if (getId() <= 0) {
        getLogger().Error("DataPointEntity::updateToDatabase - Invalid ID");
        return false;
    }
    
    try {
        std::string sql = buildUpdateSQL();
        
        auto& db_manager = getDatabaseManager();
        bool success = db_manager.executeUpdateSQLite(sql);
        
        if (success) {
            updated_at_ = std::chrono::system_clock::now();
            setState(EntityState::LOADED);
            getLogger().Info("DataPointEntity updated: " + std::to_string(getId()));
            return true;
        }
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::updateToDatabase failed: " + std::string(e.what()));
        setState(EntityState::ERROR);
    }
    
    return false;
}

bool DataPointEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        getLogger().Error("DataPointEntity::deleteFromDatabase - Invalid ID");
        return false;
    }
    
    try {
        std::ostringstream sql;
        sql << "DELETE FROM data_points WHERE id = " << getId();
        
        auto& db_manager = getDatabaseManager();
        bool success = db_manager.executeUpdateSQLite(sql.str());
        
        if (success) {
            setState(EntityState::DELETED);
            getLogger().Info("DataPointEntity deleted: " + std::to_string(getId()));
            return true;
        }
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::deleteFromDatabase failed: " + std::string(e.what()));
        setState(EntityState::ERROR);
    }
    
    return false;
}

json DataPointEntity::toJson() const {
    json j = json::object();
    
    // 기본 필드들
    j["id"] = getId();
    j["device_id"] = device_id_;
    j["name"] = name_;
    j["description"] = description_;
    j["address"] = address_;
    j["data_type"] = data_type_;
    j["access_mode"] = access_mode_;
    
    // 엔지니어링 정보
    j["unit"] = unit_;
    j["scaling_factor"] = scaling_factor_;
    j["scaling_offset"] = scaling_offset_;
    j["min_value"] = min_value_;
    j["max_value"] = max_value_;
    
    // 수집 설정
    j["is_enabled"] = is_enabled_;
    if (scan_rate_.has_value()) {
        j["scan_rate"] = scan_rate_.value();
    } else {
        j["scan_rate"] = nullptr;
    }
    j["deadband"] = deadband_;
    
    // 메타데이터
    j["config"] = config_;
    j["tags"] = tags_;
    
    // 시간 필드들
    j["created_at"] = std::chrono::duration_cast<std::chrono::seconds>(
        created_at_.time_since_epoch()).count();
    j["updated_at"] = std::chrono::duration_cast<std::chrono::seconds>(
        updated_at_.time_since_epoch()).count();
    
    return j;
}

bool DataPointEntity::fromJson(const json& data) {
    try {
        // 기본 필드들
        if (data.contains("id") && !data["id"].is_null()) {
            setId(data["id"].get<int>());
        }
        
        if (data.contains("device_id")) {
            device_id_ = data["device_id"].get<int>();
        }
        
        if (data.contains("name")) {
            name_ = data["name"].get<std::string>();
        }
        
        if (data.contains("description")) {
            description_ = data["description"].get<std::string>();
        }
        
        if (data.contains("address")) {
            address_ = data["address"].get<int>();
        }
        
        if (data.contains("data_type")) {
            data_type_ = data["data_type"].get<std::string>();
        }
        
        if (data.contains("access_mode")) {
            access_mode_ = data["access_mode"].get<std::string>();
        }
        
        // 엔지니어링 정보
        if (data.contains("unit")) {
            unit_ = data["unit"].get<std::string>();
        }
        
        if (data.contains("scaling_factor")) {
            scaling_factor_ = data["scaling_factor"].get<double>();
        }
        
        if (data.contains("scaling_offset")) {
            scaling_offset_ = data["scaling_offset"].get<double>();
        }
        
        if (data.contains("min_value")) {
            min_value_ = data["min_value"].get<double>();
        }
        
        if (data.contains("max_value")) {
            max_value_ = data["max_value"].get<double>();
        }
        
        // 수집 설정
        if (data.contains("is_enabled")) {
            is_enabled_ = data["is_enabled"].get<bool>();
        }
        
        if (data.contains("scan_rate") && !data["scan_rate"].is_null()) {
            scan_rate_ = data["scan_rate"].get<int>();
        } else {
            scan_rate_ = std::nullopt;
        }
        
        if (data.contains("deadband")) {
            deadband_ = data["deadband"].get<double>();
        }
        
        // 메타데이터
        if (data.contains("config")) {
            config_ = data["config"];
        }
        
        if (data.contains("tags") && data["tags"].is_array()) {
            tags_ = data["tags"].get<std::vector<std::string>>();
        }
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::fromJson failed: " + std::string(e.what()));
        return false;
    }
}

std::string DataPointEntity::toString() const {
    std::ostringstream oss;
    oss << "DataPoint[id=" << getId() 
        << ", device_id=" << device_id_
        << ", name='" << name_ << "'"
        << ", address=" << address_
        << ", data_type='" << data_type_ << "'"
        << ", enabled=" << (is_enabled_ ? "true" : "false")
        << "]";
    return oss.str();
}

// =======================================================================
// Worker 지원 메서드들
// =======================================================================

bool DataPointEntity::updateCurrentValue(double value, 
                                        std::optional<double> raw_value,
                                        const std::string& quality) {
    try {
        std::ostringstream sql;
        sql << "INSERT OR REPLACE INTO current_values ("
            << "point_id, value, raw_value, quality, timestamp"
            << ") VALUES ("
            << getId() << ", "
            << std::fixed << std::setprecision(4) << value << ", "
            << (raw_value.has_value() ? 
                std::to_string(raw_value.value()) : "NULL") << ", "
            << "'" << quality << "', "
            << "datetime('now', 'localtime')"
            << ")";
        
        auto& db_manager = getDatabaseManager();
        bool success = db_manager.executeUpdateSQLite(sql.str());
        
        if (success) {
            // 캐시 무효화
            cached_current_value_ = std::nullopt;
            current_value_loaded_ = false;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::updateCurrentValue failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointEntity::updateCurrentStringValue(const std::string& string_value,
                                              const std::string& quality) {
    try {
        std::ostringstream sql;
        sql << "INSERT OR REPLACE INTO current_values ("
            << "point_id, string_value, quality, timestamp"
            << ") VALUES ("
            << getId() << ", "
            << "'" << string_value << "', "
            << "'" << quality << "', "
            << "datetime('now', 'localtime')"
            << ")";
        
        auto& db_manager = getDatabaseManager();
        bool success = db_manager.executeUpdateSQLite(sql.str());
        
        if (success) {
            // 캐시 무효화
            cached_current_value_ = std::nullopt;
            current_value_loaded_ = false;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::updateCurrentStringValue failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointEntity::updateValueWithScaling(double raw_value) {
    // 스케일링 적용
    double scaled_value = applyScaling(raw_value);
    
    // 범위 검사
    if (!isValueInRange(scaled_value)) {
        getLogger().Warning("DataPointEntity::updateValueWithScaling - Value out of range: " + 
                           std::to_string(scaled_value));
        return updateCurrentValue(scaled_value, raw_value, "bad");
    }
    
    return updateCurrentValue(scaled_value, raw_value, "good");
}

bool DataPointEntity::isSignificantChange(double old_value, double new_value) const {
    if (deadband_ <= 0.0) {
        return true; // 데드밴드가 0이면 모든 변화 유효
    }
    
    double change = std::abs(new_value - old_value);
    return change >= deadband_;
}

// =======================================================================
// 프로토콜별 설정 추출
// =======================================================================

std::optional<std::string> DataPointEntity::getMQTTTopic() const {
    if (config_.contains("mqtt_topic") && config_["mqtt_topic"].is_string()) {
        return config_["mqtt_topic"].get<std::string>();
    }
    return std::nullopt;
}

std::optional<std::pair<int, int>> DataPointEntity::getBACnetObjectInfo() const {
    if (config_.contains("bacnet_object_type") && config_.contains("bacnet_instance")) {
        try {
            int obj_type = config_["bacnet_object_type"].get<int>();
            int instance = config_["bacnet_instance"].get<int>();
            return std::make_pair(obj_type, instance);
        } catch (const std::exception&) {
            // JSON 타입 변환 실패
        }
    }
    return std::nullopt;
}

json DataPointEntity::extractProtocolConfig() const {
    json protocol_config = json::object();
    
    // 기본 주소 정보
    protocol_config["address"] = address_;
    protocol_config["data_type"] = data_type_;
    protocol_config["access_mode"] = access_mode_;
    
    // config JSON에서 프로토콜별 설정 추출
    if (!config_.empty()) {
        protocol_config["protocol_config"] = config_;
    }
    
    return protocol_config;
}

// =======================================================================
// 데이터 검증 및 변환
// =======================================================================

bool DataPointEntity::isValueInRange(double value) const {
    return value >= min_value_ && value <= max_value_;
}

double DataPointEntity::applyScaling(double raw_value) const {
    return raw_value * scaling_factor_ + scaling_offset_;
}

double DataPointEntity::reverseScaling(double scaled_value) const {
    return (scaled_value - scaling_offset_) / scaling_factor_;
}

// =======================================================================
// 현재값 조회
// =======================================================================

json DataPointEntity::getCurrentValue() const {
    // 캐시 확인
    if (current_value_loaded_ && cached_current_value_.has_value()) {
        return cached_current_value_.value();
    }
    
    try {
        std::ostringstream query;
        query << "SELECT * FROM current_values WHERE point_id = " << getId();
        
        auto& db_manager = getDatabaseManager();
        auto result = db_manager.executeQuerySQLite(query.str());
        
        json current_value = json::object();
        
        if (!result.empty()) {
            const auto& row = result[0];
            for (const auto& [key, value] : row) {
                current_value[key] = value;
            }
        }
        
        // 캐시에 저장
        cached_current_value_ = current_value;
        current_value_loaded_ = true;
        
        return current_value;
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::getCurrentValue failed: " + std::string(e.what()));
        return json::object();
    }
}

std::vector<json> DataPointEntity::getValueHistory(int limit) const {
    try {
        // 실제로는 시계열 DB(InfluxDB)에서 조회해야 하지만
        // 여기서는 current_values 테이블에서 간단히 조회
        std::ostringstream query;
        query << "SELECT * FROM current_values WHERE point_id = " << getId()
              << " ORDER BY timestamp DESC LIMIT " << limit;
        
        auto& db_manager = getDatabaseManager();
        auto result = db_manager.executeQuerySQLite(query.str());
        
        std::vector<json> history;
        for (const auto& row : result) {
            json record = json::object();
            for (const auto& [key, value] : row) {
                record[key] = value;
            }
            history.push_back(record);
        }
        
        return history;
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::getValueHistory failed: " + std::string(e.what()));
        return {};
    }
}

// =======================================================================
// 관계 엔티티 로드
// =======================================================================

json DataPointEntity::getDeviceInfo() const {
    // 캐시 확인
    if (device_info_loaded_ && cached_device_info_.has_value()) {
        return cached_device_info_.value();
    }
    
    try {
        std::ostringstream query;
        query << "SELECT * FROM devices WHERE id = " << device_id_;
        
        auto& db_manager = getDatabaseManager();
        auto result = db_manager.executeQuerySQLite(query.str());
        
        json device_info = json::object();
        
        if (!result.empty()) {
            const auto& row = result[0];
            for (const auto& [key, value] : row) {
                device_info[key] = value;
            }
        }
        
        // 캐시에 저장
        cached_device_info_ = device_info;
        device_info_loaded_ = true;
        
        return device_info;
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::getDeviceInfo failed: " + std::string(e.what()));
        return json::object();
    }
}

std::vector<json> DataPointEntity::getAlarmConfigs() const {
    // 캐시 확인
    if (alarm_configs_loaded_ && cached_alarm_configs_.has_value()) {
        return cached_alarm_configs_.value();
    }
    
    try {
        // 알람 설정 테이블이 있다면 조회
        std::ostringstream query;
        query << "SELECT * FROM alarm_configs WHERE point_id = " << getId();
        
        auto& db_manager = getDatabaseManager();
        auto result = db_manager.executeQuerySQLite(query.str());
        
        std::vector<json> alarm_configs;
        for (const auto& row : result) {
            json config = json::object();
            for (const auto& [key, value] : row) {
                config[key] = value;
            }
            alarm_configs.push_back(config);
        }
        
        // 캐시에 저장
        cached_alarm_configs_ = alarm_configs;
        alarm_configs_loaded_ = true;
        
        return alarm_configs;
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::getAlarmConfigs failed: " + std::string(e.what()));
        return {};
    }
}

// =======================================================================
// 유효성 검사
// =======================================================================

bool DataPointEntity::isValid() const {
    // 필수 필드 검사
    if (device_id_ <= 0) {
        return false;
    }
    
    if (name_.empty()) {
        return false;
    }
    
    if (data_type_.empty()) {
        return false;
    }
    
    // 스케일링 팩터가 0이면 안됨
    if (scaling_factor_ == 0.0) {
        return false;
    }
    
    // 범위 검사
    if (min_value_ > max_value_) {
        return false;
    }
    
    return true;
}

// =======================================================================
// 내부 헬퍼 메서드들
// =======================================================================

bool DataPointEntity::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        // 기본 필드들
        if (row.find("device_id") != row.end()) {
            device_id_ = std::stoi(row.at("device_id"));
        }
        
        if (row.find("name") != row.end()) {
            name_ = row.at("name");
        }
        
        if (row.find("description") != row.end()) {
            description_ = row.at("description");
        }
        
        if (row.find("address") != row.end()) {
            address_ = std::stoi(row.at("address"));
        }
        
        if (row.find("data_type") != row.end()) {
            data_type_ = row.at("data_type");
        }
        
        if (row.find("access_mode") != row.end()) {
            access_mode_ = row.at("access_mode");
        }
        
        // 엔지니어링 정보
        if (row.find("unit") != row.end()) {
            unit_ = row.at("unit");
        }
        
        if (row.find("scaling_factor") != row.end() && !row.at("scaling_factor").empty()) {
            scaling_factor_ = std::stod(row.at("scaling_factor"));
        }
        
        if (row.find("scaling_offset") != row.end() && !row.at("scaling_offset").empty()) {
            scaling_offset_ = std::stod(row.at("scaling_offset"));
        }
        
        if (row.find("min_value") != row.end() && !row.at("min_value").empty()) {
            min_value_ = std::stod(row.at("min_value"));
        }
        
        if (row.find("max_value") != row.end() && !row.at("max_value").empty()) {
            max_value_ = std::stod(row.at("max_value"));
        }
        
        // 수집 설정
        if (row.find("is_enabled") != row.end()) {
            is_enabled_ = (row.at("is_enabled") == "1");
        }
        
        if (row.find("scan_rate") != row.end() && !row.at("scan_rate").empty() && row.at("scan_rate") != "NULL") {
            scan_rate_ = std::stoi(row.at("scan_rate"));
        } else {
            scan_rate_ = std::nullopt;
        }
        
        if (row.find("deadband") != row.end() && !row.at("deadband").empty()) {
            deadband_ = std::stod(row.at("deadband"));
        }
        
        // 메타데이터
        if (row.find("config") != row.end() && !row.at("config").empty()) {
            config_ = safeParseJson(row.at("config"));
        }
        
        if (row.find("tags") != row.end() && !row.at("tags").empty()) {
            tags_ = parseTagsArray(row.at("tags"));
        }
        
        setState(EntityState::LOADED);
        return true;
        
    } catch (const std::exception& e) {
        getLogger().Error("DataPointEntity::mapRowToEntity failed: " + std::string(e.what()));
        return false;
    }
}

std::string DataPointEntity::buildInsertSQL() const {
    std::ostringstream sql;
    
    sql << "INSERT INTO data_points ("
        << "device_id, name, description, address, data_type, access_mode, "
        << "unit, scaling_factor, scaling_offset, min_value, max_value, "
        << "is_enabled, scan_rate, deadband, config, tags, "
        << "created_at, updated_at"
        << ") VALUES ("
        << device_id_ << ", "
        << "'" << name_ << "', "
        << "'" << description_ << "', "
        << address_ << ", "
        << "'" << data_type_ << "', "
        << "'" << access_mode_ << "', "
        << "'" << unit_ << "', "
        << scaling_factor_ << ", "
        << scaling_offset_ << ", "
        << min_value_ << ", "
        << max_value_ << ", "
        << (is_enabled_ ? 1 : 0) << ", "
        << (scan_rate_.has_value() ? std::to_string(scan_rate_.value()) : "NULL") << ", "
        << deadband_ << ", "
        << "'" << config_.dump() << "', "
        << "'" << tagsToJsonString(tags_) << "', "
        << "datetime('now', 'localtime'), "
        << "datetime('now', 'localtime')"
        << ")";
    
    return sql.str();
}

std::string DataPointEntity::buildUpdateSQL() const {
    std::ostringstream sql;
    
    sql << "UPDATE data_points SET "
        << "device_id = " << device_id_ << ", "
        << "name = '" << name_ << "', "
        << "description = '" << description_ << "', "
        << "address = " << address_ << ", "
        << "data_type = '" << data_type_ << "', "
        << "access_mode = '" << access_mode_ << "', "
        << "unit = '" << unit_ << "', "
        << "scaling_factor = " << scaling_factor_ << ", "
        << "scaling_offset = " << scaling_offset_ << ", "
        << "min_value = " << min_value_ << ", "
        << "max_value = " << max_value_ << ", "
        << "is_enabled = " << (is_enabled_ ? 1 : 0) << ", "
        << "scan_rate = " << (scan_rate_.has_value() ? std::to_string(scan_rate_.value()) : "NULL") << ", "
        << "deadband = " << deadband_ << ", "
        << "config = '" << config_.dump() << "', "
        << "tags = '" << tagsToJsonString(tags_) << "', "
        << "updated_at = datetime('now', 'localtime') "
        << "WHERE id = " << getId();
    
    return sql.str();
}

json DataPointEntity::safeParseJson(const std::string& json_str) const {
    try {
        return json::parse(json_str);
    } catch (const std::exception&) {
        return json::object();
    }
}

std::vector<std::string> DataPointEntity::parseTagsArray(const std::string& json_array_str) const {
    try {
        json tags_json = json::parse(json_array_str);
        if (tags_json.is_array()) {
            return tags_json.get<std::vector<std::string>>();
        }
    } catch (const std::exception&) {
        // JSON 파싱 실패
    }
    return {};
}

std::string DataPointEntity::tagsToJsonString(const std::vector<std::string>& tags) const {
    json tags_json = tags;
    return tags_json.dump();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne