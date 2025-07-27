// =============================================================================
// collector/src/Database/Entities/DataPointEntity.cpp
// PulseOne 데이터포인트 엔티티 구현 - 기존 패턴 100% 준수
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
    , device_id_(0)  // device_id는 UUID를 int로 변환 필요 (별도 처리)
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
// BaseEntity 순수 가상 함수 구현
// =============================================================================

bool DataPointEntity::loadFromDatabase() {
    if (id_ <= 0) {
        logger_.Error("DataPointEntity::loadFromDatabase() - Invalid ID: " + std::to_string(id_));
        return false;
    }
    
    try {
        std::string query = "SELECT * FROM data_points WHERE id = " + std::to_string(id_);
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        if (results.empty()) {
            logger_.Warning("DataPointEntity::loadFromDatabase() - No data found for ID: " + std::to_string(id_));
            return false;
        }
        
        bool success = mapRowToEntity(results[0]);
        if (success) {
            setState(EntityState::LOADED);
            logger_.Debug("DataPointEntity loaded successfully: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::loadFromDatabase() - Exception: " + std::string(e.what()));
        setState(EntityState::ERROR);
        return false;
    }
}

bool DataPointEntity::saveToDatabase() {
    try {
        std::string query;
        
        if (getState() == EntityState::NEW) {
            // INSERT 쿼리
            query = buildInsertQuery();
        } else {
            // UPDATE 쿼리
            query = buildUpdateQuery();
        }
        
        bool success = db_manager_.isPostgresConnected() 
            ? executePostgresNonQuery(query)
            : executeSQLiteNonQuery(query);
        
        if (success) {
            // NEW 상태였다면 ID 조회 (AUTO_INCREMENT)
            if (getState() == EntityState::NEW && id_ <= 0) {
                std::string last_id_query = db_manager_.isPostgresConnected()
                    ? "SELECT lastval()"
                    : "SELECT last_insert_rowid()";
                
                auto result = db_manager_.isPostgresConnected()
                    ? executePostgresQuery(last_id_query)
                    : executeSQLiteQuery(last_id_query);
                
                if (!result.empty() && result[0].find("lastval") != result[0].end()) {
                    id_ = std::stoi(result[0].at("lastval"));
                } else if (!result.empty() && result[0].find("last_insert_rowid()") != result[0].end()) {
                    id_ = std::stoi(result[0].at("last_insert_rowid()"));
                }
            }
            
            setState(EntityState::LOADED);
            updated_at_ = std::chrono::system_clock::now();
            logger_.Info("DataPointEntity saved successfully: " + name_);
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::saveToDatabase() - Exception: " + std::string(e.what()));
        setState(EntityState::ERROR);
        return false;
    }
}

bool DataPointEntity::deleteFromDatabase() {
    if (id_ <= 0) {
        logger_.Error("DataPointEntity::deleteFromDatabase() - Invalid ID: " + std::to_string(id_));
        return false;
    }
    
    try {
        std::string query = "DELETE FROM data_points WHERE id = " + std::to_string(id_);
        
        bool success = db_manager_.isPostgresConnected() 
            ? executePostgresNonQuery(query)
            : executeSQLiteNonQuery(query);
        
        if (success) {
            setState(EntityState::DELETED);
            logger_.Info("DataPointEntity deleted successfully: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::deleteFromDatabase() - Exception: " + std::string(e.what()));
        setState(EntityState::ERROR);
        return false;
    }
}

bool DataPointEntity::updateToDatabase() {
    if (id_ <= 0 || !isValid()) {
        logger_.Error("DataPointEntity::updateToDatabase - Invalid data point data or ID");
        return false;
    }
    
    try {
        auto row_data = mapEntityToRow();
        std::string set_clause;
        
        for (const auto& pair : row_data) {
            if (!set_clause.empty()) set_clause += ", ";
            set_clause += pair.first + " = '" + escapeString(pair.second) + "'";
        }
        
        std::string query = "UPDATE data_points SET " + set_clause + " WHERE id = " + std::to_string(id_);
        
        bool success = executeUnifiedNonQuery(query);
        
        if (success) {
            setState(EntityState::LOADED);
            updated_at_ = std::chrono::system_clock::now();
            logger_.Info("DataPointEntity updated successfully: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointEntity::updateToDatabase() - Exception: " + std::string(e.what()));
        setState(EntityState::ERROR);
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
        
        // 시간 정보
        auto created_time_t = std::chrono::system_clock::to_time_t(created_at_);
        auto updated_time_t = std::chrono::system_clock::to_time_t(updated_at_);
        auto last_read_time_t = std::chrono::system_clock::to_time_t(last_read_time_);
        auto last_write_time_t = std::chrono::system_clock::to_time_t(last_write_time_);
        
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&created_time_t), "%Y-%m-%d %H:%M:%S");
        j["created_at"] = ss.str();
        
        ss.str("");
        ss << std::put_time(std::gmtime(&updated_time_t), "%Y-%m-%d %H:%M:%S");
        j["updated_at"] = ss.str();
        
        ss.str("");
        ss << std::put_time(std::gmtime(&last_read_time_t), "%Y-%m-%d %H:%M:%S");
        j["last_read_time"] = ss.str();
        
        ss.str("");
        ss << std::put_time(std::gmtime(&last_write_time_t), "%Y-%m-%d %H:%M:%S");
        j["last_write_time"] = ss.str();
        
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

// =============================================================================
// 고급 기능 (DeviceEntity와 동일 패턴)
// =============================================================================

DataPoint DataPointEntity::toDataPointStruct() const {
    DataPoint dp;
    
    // 기본 정보 매핑
    dp.id = std::to_string(id_);  // INTEGER ID를 문자열로 변환
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
    json config;
    
    config["basic"] = {
        {"name", name_},
        {"description", description_},
        {"address", address_},
        {"data_type", data_type_},
        {"access_mode", access_mode_},
        {"is_enabled", is_enabled_}
    };
    
    config["engineering"] = {
        {"unit", unit_},
        {"scaling_factor", scaling_factor_},
        {"scaling_offset", scaling_offset_},
        {"min_value", min_value_},
        {"max_value", max_value_}
    };
    
    config["logging"] = {
        {"log_enabled", log_enabled_},
        {"log_interval_ms", log_interval_ms_},
        {"log_deadband", log_deadband_}
    };
    
    config["metadata"] = {
        {"tags", tags_},
        {"custom", metadata_}
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
    
    // Worker가 필요한 최소 정보만 포함
    context["scaling"] = {
        {"factor", scaling_factor_},
        {"offset", scaling_offset_}
    };
    
    context["limits"] = {
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
    
    // Redis 키 생성용 정보
    rdb_context["redis_key"] = "data:device:" + std::to_string(device_id_) + ":point:" + std::to_string(id_);
    rdb_context["time_series_key"] = "ts:" + std::to_string(device_id_) + ":" + std::to_string(id_);
    
    // 로깅 설정
    rdb_context["logging"] = {
        {"enabled", log_enabled_},
        {"interval_ms", log_interval_ms_},
        {"deadband", log_deadband_}
    };
    
    return rdb_context;
}

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
    
    // 접근 모드 검사
    if (access_mode_ != "read" && access_mode_ != "write" && access_mode_ != "read_write") {
        return false;
    }
    
    return true;
}

// =============================================================================
// 내부 헬퍼 메서드들
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
        
        // 메타데이터 파싱 (JSON 형태로 저장되어 있다고 가정)
        if (row.find("tags") != row.end() && !row.at("tags").empty()) {
            try {
                json tags_json = json::parse(row.at("tags"));
                if (tags_json.is_array()) {
                    tags_ = tags_json;
                }
            } catch (...) {
                // JSON 파싱 실패 시 무시
            }
        }
        
        if (row.find("metadata") != row.end() && !row.at("metadata").empty()) {
            try {
                metadata_ = json::parse(row.at("metadata"));
            } catch (...) {
                // JSON 파싱 실패 시 무시
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

std::map<std::string, std::string> DataPointEntity::mapEntityToRow() const {
    std::map<std::string, std::string> row;
    
    // 기본 필드들
    row["device_id"] = std::to_string(device_id_);
    row["name"] = name_;
    row["description"] = description_;
    row["address"] = std::to_string(address_);
    row["data_type"] = data_type_;
    row["access_mode"] = access_mode_;
    row["is_enabled"] = is_enabled_ ? "1" : "0";
    
    // 엔지니어링 정보
    row["unit"] = unit_;
    row["scaling_factor"] = std::to_string(scaling_factor_);
    row["scaling_offset"] = std::to_string(scaling_offset_);
    row["min_value"] = std::to_string(min_value_);
    row["max_value"] = std::to_string(max_value_);
    
    // 로깅 설정
    row["log_enabled"] = log_enabled_ ? "1" : "0";
    row["log_interval_ms"] = std::to_string(log_interval_ms_);
    row["log_deadband"] = std::to_string(log_deadband_);
    
    // 메타데이터 (JSON 문자열로 저장)
    row["tags"] = json(tags_).dump();
    row["metadata"] = metadata_.dump();
    
    // 통계 정보
    row["read_count"] = std::to_string(read_count_);
    row["write_count"] = std::to_string(write_count_);
    row["error_count"] = std::to_string(error_count_);
    
    return row;
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne