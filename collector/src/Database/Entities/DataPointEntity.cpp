// =============================================================================
// collector/src/Database/Entities/DataPointEntity.cpp
// PulseOne 데이터포인트 엔티티 구현 - DeviceEntity 패턴 100% 준수
// =============================================================================

#include "Database/Entities/DataPointEntity.h"
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

DataPointEntity::DataPointEntity() 
    : BaseEntity<DataPointEntity>()
    , device_id_(0)
    , name_("")
    , description_("")
    , address_(0)
    , data_type_("UNKNOWN")
    , access_mode_("read")
    , is_enabled_(true)
    , unit_("")
    , scaling_factor_(1.0)
    , scaling_offset_(0.0)
    , min_value_(std::numeric_limits<double>::lowest())
    , max_value_(std::numeric_limits<double>::max())
    , log_enabled_(true)
    , log_interval_ms_(0)
    , log_deadband_(0.0)
    , last_read_time_(std::chrono::system_clock::now())
    , last_write_time_(std::chrono::system_clock::now())
    , read_count_(0)
    , write_count_(0)
    , error_count_(0) {
}

DataPointEntity::DataPointEntity(int point_id) 
    : BaseEntity<DataPointEntity>(point_id)
    , device_id_(0)
    , name_("")
    , description_("")
    , address_(0)
    , data_type_("UNKNOWN")
    , access_mode_("read")
    , is_enabled_(true)
    , unit_("")
    , scaling_factor_(1.0)
    , scaling_offset_(0.0)
    , min_value_(std::numeric_limits<double>::lowest())
    , max_value_(std::numeric_limits<double>::max())
    , log_enabled_(true)
    , log_interval_ms_(0)
    , log_deadband_(0.0)
    , last_read_time_(std::chrono::system_clock::now())
    , last_write_time_(std::chrono::system_clock::now())
    , read_count_(0)
    , write_count_(0)
    , error_count_(0) {
}

DataPointEntity::DataPointEntity(const DataPoint& data_point) 
    : BaseEntity<DataPointEntity>()
    , device_id_(0)
    , name_(data_point.name)
    , description_(data_point.description)
    , address_(data_point.address)
    , data_type_(data_point.data_type)
    , access_mode_(data_point.is_writable ? "read_write" : "read")
    , is_enabled_(data_point.is_enabled)
    , unit_(data_point.unit)
    , scaling_factor_(data_point.scaling_factor)
    , scaling_offset_(data_point.scaling_offset)
    , min_value_(data_point.min_value)
    , max_value_(data_point.max_value)
    , log_enabled_(data_point.log_enabled)
    , log_interval_ms_(data_point.log_interval_ms)
    , log_deadband_(data_point.log_deadband)
    , tags_(data_point.tags)
    , metadata_(data_point.metadata)
    , last_read_time_(std::chrono::system_clock::now())
    , last_write_time_(std::chrono::system_clock::now())
    , read_count_(data_point.read_count)
    , write_count_(data_point.write_count)
    , error_count_(data_point.error_count) {
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (DeviceEntity 패턴)
// =============================================================================

bool DataPointEntity::loadFromDatabase() {
    if (id_ <= 0) {
        logger_.Error("DataPointEntity::loadFromDatabase - Invalid data point ID: " + std::to_string(id_));
        markError();
        return false;
    }
    
    try {
        std::string query = "SELECT * FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        // 🔥 DeviceEntity와 동일한 방식으로 executeUnifiedQuery 사용
        auto results = executeUnifiedQuery(query);
        
        if (results.empty()) {
            logger_.Warn("DataPointEntity::loadFromDatabase - Data point not found: " + std::to_string(id_));
            return false;
        }
        
        // 첫 번째 행을 엔티티로 변환
        bool success = mapRowToEntity(results[0]);
        
        if (success) {
            markSaved();  // DeviceEntity 패턴
            logger_.Info("DataPointEntity::loadFromDatabase - Loaded data point: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool DataPointEntity::saveToDatabase() {
    if (!isValid()) {
        logger_.Error("DataPointEntity::saveToDatabase - Invalid data point data");
        return false;
    }
    
    try {
        std::string sql = buildInsertSQL();  // DeviceEntity 패턴
        
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
            
            markSaved();  // DeviceEntity 패턴
            logger_.Info("DataPointEntity::saveToDatabase - Saved data point: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool DataPointEntity::updateToDatabase() {
    if (id_ <= 0 || !isValid()) {
        logger_.Error("DataPointEntity::updateToDatabase - Invalid data point data or ID");
        return false;
    }
    
    try {
        std::string sql = buildUpdateSQL();  // DeviceEntity 패턴
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markSaved();  // DeviceEntity 패턴
            logger_.Info("DataPointEntity::updateToDatabase - Updated data point: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::updateToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool DataPointEntity::deleteFromDatabase() {
    if (id_ <= 0) {
        logger_.Error("DataPointEntity::deleteFromDatabase - Invalid data point ID");
        return false;
    }
    
    try {
        std::string sql = "DELETE FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markDeleted();  // DeviceEntity 패턴
            logger_.Info("DataPointEntity::deleteFromDatabase - Deleted data point: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::deleteFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

json DataPointEntity::toJson() const {
    json j;
    
    try {
        // 기본 정보
        j["id"] = id_;
        j["device_id"] = device_id_;
        j["name"] = name_;
        j["description"] = description_;
        
        // 주소 및 타입 정보
        j["address"] = address_;
        j["data_type"] = data_type_;
        j["access_mode"] = access_mode_;
        j["is_enabled"] = is_enabled_;
        
        // 엔지니어링 정보
        j["unit"] = unit_;
        j["scaling_factor"] = scaling_factor_;
        j["scaling_offset"] = scaling_offset_;
        j["min_value"] = min_value_;
        j["max_value"] = max_value_;
        
        // 로깅 설정
        j["log_enabled"] = log_enabled_;
        j["log_interval_ms"] = log_interval_ms_;
        j["log_deadband"] = log_deadband_;
        
        // 메타데이터
        j["tags"] = tags_;
        j["metadata"] = metadata_;
        
        // 통계 정보
        j["read_count"] = read_count_;
        j["write_count"] = write_count_;
        j["error_count"] = error_count_;
        
        // 시간 정보 (DeviceEntity 패턴)
        j["created_at"] = timestampToString(created_at_);
        j["updated_at"] = timestampToString(updated_at_);
        j["last_read_time"] = timestampToString(last_read_time_);
        j["last_write_time"] = timestampToString(last_write_time_);
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::toJson() - Exception: " + std::string(e.what()));
    }
    
    return j;
}

bool DataPointEntity::fromJson(const json& j) {
    try {
        // 기본 정보
        if (j.contains("id") && j["id"].is_number_integer()) {
            id_ = j["id"];
        }
        
        if (j.contains("device_id") && j["device_id"].is_number_integer()) {
            device_id_ = j["device_id"];
        }
        
        if (j.contains("name") && j["name"].is_string()) {
            name_ = j["name"];
        }
        
        if (j.contains("description") && j["description"].is_string()) {
            description_ = j["description"];
        }
        
        // 주소 및 타입 정보
        if (j.contains("address") && j["address"].is_number_integer()) {
            address_ = j["address"];
        }
        
        if (j.contains("data_type") && j["data_type"].is_string()) {
            data_type_ = j["data_type"];
        }
        
        if (j.contains("access_mode") && j["access_mode"].is_string()) {
            access_mode_ = j["access_mode"];
        }
        
        if (j.contains("is_enabled") && j["is_enabled"].is_boolean()) {
            is_enabled_ = j["is_enabled"];
        }
        
        // 엔지니어링 정보
        if (j.contains("unit") && j["unit"].is_string()) {
            unit_ = j["unit"];
        }
        
        if (j.contains("scaling_factor") && j["scaling_factor"].is_number()) {
            scaling_factor_ = j["scaling_factor"];
        }
        
        if (j.contains("scaling_offset") && j["scaling_offset"].is_number()) {
            scaling_offset_ = j["scaling_offset"];
        }
        
        if (j.contains("min_value") && j["min_value"].is_number()) {
            min_value_ = j["min_value"];
        }
        
        if (j.contains("max_value") && j["max_value"].is_number()) {
            max_value_ = j["max_value"];
        }
        
        // 로깅 설정
        if (j.contains("log_enabled") && j["log_enabled"].is_boolean()) {
            log_enabled_ = j["log_enabled"];
        }
        
        if (j.contains("log_interval_ms") && j["log_interval_ms"].is_number_integer()) {
            log_interval_ms_ = j["log_interval_ms"];
        }
        
        if (j.contains("log_deadband") && j["log_deadband"].is_number()) {
            log_deadband_ = j["log_deadband"];
        }
        
        // 메타데이터
        if (j.contains("tags") && j["tags"].is_array()) {
            tags_ = j["tags"];
        }
        
        if (j.contains("metadata") && j["metadata"].is_object()) {
            metadata_ = j["metadata"];
        }
        
        // 통계 정보
        if (j.contains("read_count") && j["read_count"].is_number_unsigned()) {
            read_count_ = j["read_count"];
        }
        
        if (j.contains("write_count") && j["write_count"].is_number_unsigned()) {
            write_count_ = j["write_count"];
        }
        
        if (j.contains("error_count") && j["error_count"].is_number_unsigned()) {
            error_count_ = j["error_count"];
        }
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::fromJson() - Exception: " + std::string(e.what()));
        return false;
    }
}

std::string DataPointEntity::toString() const {
    std::stringstream ss;
    ss << "DataPointEntity{";
    ss << "id=" << id_;
    ss << ", device_id=" << device_id_;
    ss << ", name='" << name_ << "'";
    ss << ", address=" << address_;
    ss << ", data_type='" << data_type_ << "'";
    ss << ", enabled=" << (is_enabled_ ? "true" : "false");
    ss << "}";
    return ss.str();
}

std::string DataPointEntity::getTableName() const {
    return "data_points";
}

// =============================================================================
// 고급 기능 (DeviceEntity와 동일 패턴)
// =============================================================================

DataPoint DataPointEntity::toDataPointStruct() const {
    DataPoint dp;
    
    // 기본 정보 매핑
    dp.id = std::to_string(id_);
    dp.device_id = std::to_string(device_id_);
    dp.name = name_;
    dp.description = description_;
    
    // 주소 정보
    dp.address = address_;
    dp.address_string = std::to_string(address_);
    
    // 데이터 타입 및 설정
    dp.data_type = data_type_;
    dp.is_enabled = is_enabled_;
    dp.is_writable = (access_mode_ == "write" || access_mode_ == "read_write");
    
    // 엔지니어링 정보
    dp.unit = unit_;
    dp.scaling_factor = scaling_factor_;
    dp.scaling_offset = scaling_offset_;
    dp.min_value = min_value_;
    dp.max_value = max_value_;
    
    // 로깅 설정
    dp.log_enabled = log_enabled_;
    dp.log_interval_ms = log_interval_ms_;
    dp.log_deadband = log_deadband_;
    
    // 메타데이터
    dp.tags = tags_;
    dp.metadata = metadata_;
    
    // 통계 정보
    dp.read_count = read_count_;
    dp.write_count = write_count_;
    dp.error_count = error_count_;
    
    // 시간 정보
    dp.created_at = created_at_;
    dp.updated_at = updated_at_;
    dp.last_read_time = last_read_time_;
    dp.last_write_time = last_write_time_;
    
    return dp;
}

json DataPointEntity::extractConfiguration() const {
    json config = {
        {"basic", {
            {"name", name_},
            {"description", description_},
            {"address", address_},
            {"data_type", data_type_},
            {"access_mode", access_mode_},
            {"is_enabled", is_enabled_}
        }},
        {"engineering", {
            {"unit", unit_},
            {"scaling_factor", scaling_factor_},
            {"scaling_offset", scaling_offset_},
            {"min_value", min_value_},
            {"max_value", max_value_}
        }},
        {"logging", {
            {"log_enabled", log_enabled_},
            {"log_interval_ms", log_interval_ms_},
            {"log_deadband", log_deadband_}
        }},
        {"metadata", {
            {"tags", tags_},
            {"custom", metadata_}
        }}
    };
    
    return config;
}

json DataPointEntity::getWorkerContext() const {
    json context;
    context["point_id"] = id_;
    context["device_id"] = device_id_;
    context["name"] = name_;
    context["address"] = address_;
    context["data_type"] = data_type_;
    context["is_enabled"] = is_enabled_;
    context["is_writable"] = (access_mode_ == "write" || access_mode_ == "read_write");
    
    // 스케일링 정보
    context["scaling"] = {
        {"factor", scaling_factor_},
        {"offset", scaling_offset_}
    };
    
    // 범위 정보
    context["range"] = {
        {"min", min_value_},
        {"max", max_value_}
    };
    
    return context;
}

json DataPointEntity::getRDBContext() const {
    json rdb_context;
    rdb_context["point_id"] = id_;
    rdb_context["device_id"] = device_id_;
    rdb_context["name"] = name_;
    rdb_context["data_type"] = data_type_;
    rdb_context["unit"] = unit_;
    
    // 로깅 설정만 포함
    rdb_context["logging"] = {
        {"enabled", log_enabled_},
        {"interval_ms", log_interval_ms_},
        {"deadband", log_deadband_}
    };
    
    return rdb_context;
}

bool DataPointEntity::isValid() const {
    // 기본 유효성 검사
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
    
    // 접근 모드 검사
    if (access_mode_ != "read" && access_mode_ != "write" && access_mode_ != "read_write") {
        return false;
    }
    
    return true;
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceEntity 패턴)
// =============================================================================

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
        
        if (row.find("is_enabled") != row.end()) {
            is_enabled_ = (row.at("is_enabled") == "1" || row.at("is_enabled") == "true");
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
        
        // 로깅 설정
        if (row.find("log_enabled") != row.end()) {
            log_enabled_ = (row.at("log_enabled") == "1" || row.at("log_enabled") == "true");
        }
        
        if (row.find("log_interval_ms") != row.end() && !row.at("log_interval_ms").empty()) {
            log_interval_ms_ = std::stoi(row.at("log_interval_ms"));
        }
        
        if (row.find("log_deadband") != row.end() && !row.at("log_deadband").empty()) {
            log_deadband_ = std::stod(row.at("log_deadband"));
        }
        
        // 메타데이터 처리
        if (row.find("tags") != row.end() && !row.at("tags").empty()) {
            try {
                json tags_json = json::parse(row.at("tags"));
                if (tags_json.is_array()) {
                    tags_ = tags_json;
                }
            } catch (...) {
                // JSON 파싱 실패시 무시
            }
        }
        
        if (row.find("metadata") != row.end() && !row.at("metadata").empty()) {
            try {
                metadata_ = json::parse(row.at("metadata"));
            } catch (...) {
                // JSON 파싱 실패시 무시
            }
        }
        
        // 통계 정보
        if (row.find("read_count") != row.end() && !row.at("read_count").empty()) {
            read_count_ = std::stoull(row.at("read_count"));
        }
        
        if (row.find("write_count") != row.end() && !row.at("write_count").empty()) {
            write_count_ = std::stoull(row.at("write_count"));
        }
        
        if (row.find("error_count") != row.end() && !row.at("error_count").empty()) {
            error_count_ = std::stoull(row.at("error_count"));
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::mapRowToEntity() - Exception: " + std::string(e.what()));
        return false;
    }
}

std::string DataPointEntity::buildInsertSQL() const {
    std::stringstream ss;
    
    ss << "INSERT INTO " << getTableName() << " (";
    ss << "device_id, name, description, address, data_type, access_mode, is_enabled, ";
    ss << "unit, scaling_factor, scaling_offset, min_value, max_value, ";
    ss << "log_enabled, log_interval_ms, log_deadband, tags, metadata, ";
    ss << "read_count, write_count, error_count";
    ss << ") VALUES (";
    ss << device_id_ << ", ";
    ss << "'" << escapeString(name_) << "', ";
    ss << "'" << escapeString(description_) << "', ";
    ss << address_ << ", ";
    ss << "'" << escapeString(data_type_) << "', ";
    ss << "'" << escapeString(access_mode_) << "', ";
    ss << (is_enabled_ ? 1 : 0) << ", ";
    ss << "'" << escapeString(unit_) << "', ";
    ss << scaling_factor_ << ", ";
    ss << scaling_offset_ << ", ";
    ss << min_value_ << ", ";
    ss << max_value_ << ", ";
    ss << (log_enabled_ ? 1 : 0) << ", ";
    ss << log_interval_ms_ << ", ";
    ss << log_deadband_ << ", ";
    ss << "'" << json(tags_).dump() << "', ";
    ss << "'" << metadata_.dump() << "', ";
    ss << read_count_ << ", ";
    ss << write_count_ << ", ";
    ss << error_count_;
    ss << ")";
    
    return ss.str();
}

std::string DataPointEntity::buildUpdateSQL() const {
    std::stringstream ss;
    
    ss << "UPDATE " << getTableName() << " SET ";
    ss << "device_id = " << device_id_ << ", ";
    ss << "name = '" << escapeString(name_) << "', ";
    ss << "description = '" << escapeString(description_) << "', ";
    ss << "address = " << address_ << ", ";
    ss << "data_type = '" << escapeString(data_type_) << "', ";
    ss << "access_mode = '" << escapeString(access_mode_) << "', ";
    ss << "is_enabled = " << (is_enabled_ ? 1 : 0) << ", ";
    ss << "unit = '" << escapeString(unit_) << "', ";
    ss << "scaling_factor = " << scaling_factor_ << ", ";
    ss << "scaling_offset = " << scaling_offset_ << ", ";
    ss << "min_value = " << min_value_ << ", ";
    ss << "max_value = " << max_value_ << ", ";
    ss << "log_enabled = " << (log_enabled_ ? 1 : 0) << ", ";
    ss << "log_interval_ms = " << log_interval_ms_ << ", ";
    ss << "log_deadband = " << log_deadband_ << ", ";
    ss << "tags = '" << json(tags_).dump() << "', ";
    ss << "metadata = '" << metadata_.dump() << "', ";
    ss << "read_count = " << read_count_ << ", ";
    ss << "write_count = " << write_count_ << ", ";
    ss << "error_count = " << error_count_ << " ";
    ss << "WHERE id = " << id_;
    
    return ss.str();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne