// =============================================================================
// collector/include/Database/SQLQueries.h
// 🎯 SQL 쿼리 상수 중앙 관리 - 모든 쿼리를 한 곳에서 관리
// =============================================================================

#ifndef SQL_QUERIES_H
#define SQL_QUERIES_H

#include <string>

namespace PulseOne {
namespace Database {
namespace SQL {

// =============================================================================
// 🎯 DeviceRepository 쿼리들
// =============================================================================
namespace Device {
    
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        ORDER BY id
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE id = ?
    )";
    
    const std::string FIND_BY_PROTOCOL = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE protocol_type = ? AND is_enabled = 1
        ORDER BY name
    )";
    
    const std::string FIND_ENABLED = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE is_enabled = 1
        ORDER BY protocol_type, name
    )";
    
    const std::string INSERT = R"(
        INSERT INTO devices (
            tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE devices SET 
            tenant_id = ?, site_id = ?, device_group_id = ?, edge_server_id = ?,
            name = ?, description = ?, device_type = ?, manufacturer = ?, model = ?, 
            serial_number = ?, protocol_type = ?, endpoint = ?, config = ?, 
            is_enabled = ?, installation_date = ?, last_maintenance = ?, 
            updated_at = ?
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM devices WHERE id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) FROM devices";
    
    const std::string COUNT_BY_PROTOCOL = "SELECT COUNT(*) FROM devices WHERE protocol_type = ?";
    
} // namespace Device

// =============================================================================
// 🎯 DataPointRepository 쿼리들
// =============================================================================
namespace DataPoint {
    
    const std::string FIND_ALL = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        ORDER BY device_id, address
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE id = ?
    )";
    
    const std::string FIND_BY_DEVICE_ID = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ?
        ORDER BY address
    )";
    
    const std::string FIND_ENABLED_BY_DEVICE = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ? AND is_enabled = 1
        ORDER BY address
    )";
    
    const std::string FIND_BY_DATA_TYPE = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE data_type = ?
        ORDER BY device_id, address
    )";
    
    const std::string INSERT = R"(
        INSERT INTO data_points (
            device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE data_points SET 
            device_id = ?, name = ?, description = ?, address = ?, data_type = ?, 
            access_mode = ?, is_enabled = ?, unit = ?, scaling_factor = ?, 
            scaling_offset = ?, min_value = ?, max_value = ?, log_enabled = ?, 
            log_interval_ms = ?, log_deadband = ?, tags = ?, metadata = ?, 
            updated_at = ?
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM data_points WHERE id = ?";
    
    const std::string DELETE_BY_DEVICE_ID = "DELETE FROM data_points WHERE device_id = ?";
    
    const std::string COUNT_BY_DEVICE = "SELECT COUNT(*) FROM data_points WHERE device_id = ?";
    
} // namespace DataPoint

// =============================================================================
// 🎯 DeviceSettingsRepository 쿼리들
// =============================================================================
namespace DeviceSettings {
    
    const std::string FIND_ALL = R"(
        SELECT 
            device_id, polling_interval_ms, connection_timeout_ms, max_retry_count,
            retry_interval_ms, backoff_time_ms, keep_alive_enabled, keep_alive_interval_s,
            scan_rate_override, read_timeout_ms, write_timeout_ms, backoff_multiplier,
            max_backoff_time_ms, keep_alive_timeout_s, data_validation_enabled,
            performance_monitoring_enabled, diagnostic_mode_enabled, updated_at
        FROM device_settings 
        ORDER BY device_id
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            device_id, polling_interval_ms, connection_timeout_ms, max_retry_count,
            retry_interval_ms, backoff_time_ms, keep_alive_enabled, keep_alive_interval_s,
            scan_rate_override, read_timeout_ms, write_timeout_ms, backoff_multiplier,
            max_backoff_time_ms, keep_alive_timeout_s, data_validation_enabled,
            performance_monitoring_enabled, diagnostic_mode_enabled, updated_at
        FROM device_settings 
        WHERE device_id = ?
    )";
    
    const std::string UPSERT = R"(
        INSERT OR REPLACE INTO device_settings (
            device_id, polling_interval_ms, connection_timeout_ms, max_retry_count,
            retry_interval_ms, backoff_time_ms, keep_alive_enabled, keep_alive_interval_s,
            scan_rate_override, read_timeout_ms, write_timeout_ms, backoff_multiplier,
            max_backoff_time_ms, keep_alive_timeout_s, data_validation_enabled,
            performance_monitoring_enabled, diagnostic_mode_enabled, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM device_settings WHERE device_id = ?";
    
} // namespace DeviceSettings

// =============================================================================
// 🎯 CurrentValueRepository 쿼리들  
// =============================================================================
namespace CurrentValue {
    
    const std::string FIND_ALL = R"(
        SELECT 
            point_id, value, raw_value, string_value, quality,
            timestamp, updated_at
        FROM current_values 
        ORDER BY point_id
    )";
    
    const std::string FIND_BY_POINT_ID = R"(
        SELECT 
            point_id, value, raw_value, string_value, quality,
            timestamp, updated_at
        FROM current_values 
        WHERE point_id = ?
    )";
    
    const std::string FIND_BY_DEVICE_ID = R"(
        SELECT 
            cv.point_id, cv.value, cv.raw_value, cv.string_value, cv.quality,
            cv.timestamp, cv.updated_at
        FROM current_values cv
        JOIN data_points dp ON cv.point_id = dp.id
        WHERE dp.device_id = ?
        ORDER BY dp.address
    )";
    
    const std::string UPSERT_VALUE = R"(
        INSERT OR REPLACE INTO current_values (
            point_id, value, raw_value, string_value, quality, timestamp, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string DELETE_BY_POINT_ID = "DELETE FROM current_values WHERE point_id = ?";
    
    const std::string GET_LATEST_VALUES = R"(
        SELECT 
            cv.point_id, dp.name as point_name, cv.value, cv.quality, cv.timestamp,
            d.name as device_name, d.protocol_type
        FROM current_values cv
        JOIN data_points dp ON cv.point_id = dp.id
        JOIN devices d ON dp.device_id = d.id
        WHERE cv.timestamp >= ?
        ORDER BY cv.timestamp DESC
    )";
    
} // namespace CurrentValue

// =============================================================================
// 🎯 기타 공통 쿼리들
// =============================================================================
namespace Common {
    
    const std::string CHECK_TABLE_EXISTS = R"(
        SELECT name FROM sqlite_master 
        WHERE type='table' AND name = ?
    )";
    
    const std::string GET_TABLE_INFO = "PRAGMA table_info(?)";
    
    const std::string GET_FOREIGN_KEYS = "PRAGMA foreign_key_list(?)";
    
    const std::string ENABLE_FOREIGN_KEYS = "PRAGMA foreign_keys = ON";
    
    const std::string DISABLE_FOREIGN_KEYS = "PRAGMA foreign_keys = OFF";
    
    const std::string BEGIN_TRANSACTION = "BEGIN TRANSACTION";
    
    const std::string COMMIT_TRANSACTION = "COMMIT";
    
    const std::string ROLLBACK_TRANSACTION = "ROLLBACK";
    
} // namespace Common

// =============================================================================
// 🎯 동적 쿼리 빌더 헬퍼들
// =============================================================================
namespace QueryBuilder {
    
    /**
     * @brief WHERE 절 생성 헬퍼
     * @param conditions 조건들 (column=value 형태)
     * @return WHERE 절 문자열
     */
    inline std::string buildWhereClause(const std::vector<std::pair<std::string, std::string>>& conditions) {
        if (conditions.empty()) return "";
        
        std::string where_clause = " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) where_clause += " AND ";
            where_clause += conditions[i].first + " = '" + conditions[i].second + "'";
        }
        return where_clause;
    }
    
    /**
     * @brief ORDER BY 절 생성 헬퍼
     * @param column 정렬할 컬럼
     * @param direction ASC 또는 DESC
     * @return ORDER BY 절 문자열
     */
    inline std::string buildOrderByClause(const std::string& column, const std::string& direction = "ASC") {
        return " ORDER BY " + column + " " + direction;
    }
    
    /**
     * @brief LIMIT 절 생성 헬퍼
     * @param limit 제한 수
     * @param offset 오프셋 (기본값: 0)
     * @return LIMIT 절 문자열
     */
    inline std::string buildLimitClause(int limit, int offset = 0) {
        if (offset > 0) {
            return " LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);
        } else {
            return " LIMIT " + std::to_string(limit);
        }
    }
    
} // namespace QueryBuilder

} // namespace SQL
} // namespace Database  
} // namespace PulseOne

#endif // SQL_QUERIES_H