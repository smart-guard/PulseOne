// =============================================================================
// collector/include/Database/SQLQueries.h
// 🎯 SQL 쿼리 상수 중앙 관리 - 모든 쿼리를 한 곳에서 관리 (완전판)
// =============================================================================

#ifndef SQL_QUERIES_H
#define SQL_QUERIES_H

#include <string>

namespace PulseOne {
namespace Database {
namespace SQL {

// =============================================================================
// 🎯 DeviceRepository 쿼리들 (완전판)
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
    
    const std::string FIND_BY_TENANT = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE tenant_id = ?
        ORDER BY name
    )";
    
    const std::string FIND_BY_SITE = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE site_id = ?
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
    
    const std::string FIND_DISABLED = R"(
        SELECT 
            id, tenant_id, site_id, device_group_id, edge_server_id,
            name, description, device_type, manufacturer, model, serial_number,
            protocol_type, endpoint, config, is_enabled, installation_date, 
            last_maintenance, created_by, created_at, updated_at
        FROM devices 
        WHERE is_enabled = 0
        ORDER BY updated_at DESC
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
    
    const std::string UPDATE_STATUS = R"(
        UPDATE devices 
        SET is_enabled = ?, updated_at = ?
        WHERE id = ?
    )";
    
    const std::string UPDATE_ENDPOINT = R"(
        UPDATE devices 
        SET endpoint = ?, updated_at = ?
        WHERE id = ?
    )";
    
    const std::string UPDATE_CONFIG = R"(
        UPDATE devices 
        SET config = ?, updated_at = ?
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM devices WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM devices WHERE id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM devices";
    
    const std::string COUNT_ENABLED = "SELECT COUNT(*) as count FROM devices WHERE is_enabled = 1";
    
    const std::string COUNT_BY_PROTOCOL = "SELECT COUNT(*) as count FROM devices WHERE protocol_type = ?";
    
    const std::string GET_PROTOCOL_DISTRIBUTION = R"(
        SELECT protocol_type, COUNT(*) as count 
        FROM devices 
        GROUP BY protocol_type
        ORDER BY count DESC
    )";
    
    const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";
    
    // 테이블 생성
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            site_id INTEGER NOT NULL,
            device_group_id INTEGER,
            edge_server_id INTEGER,
            
            -- 디바이스 기본 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            device_type VARCHAR(50) NOT NULL,
            manufacturer VARCHAR(100),
            model VARCHAR(100),
            serial_number VARCHAR(100),
            
            -- 통신 설정
            protocol_type VARCHAR(50) NOT NULL,
            endpoint VARCHAR(255) NOT NULL,
            config TEXT NOT NULL,
            
            -- 상태 정보
            is_enabled INTEGER DEFAULT 1,
            installation_date DATE,
            last_maintenance DATE,
            
            created_by INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
            FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
            FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
            FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
            FOREIGN KEY (created_by) REFERENCES users(id)
        )
    )";
    
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
    
    // 🔥 누락된 쿼리 1: FIND_BY_DEVICE_ID_ENABLED
    const std::string FIND_BY_DEVICE_ID_ENABLED = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ? AND is_enabled = 1
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
    
    // 🔥 누락된 쿼리 2: FIND_BY_DEVICE_AND_ADDRESS
    const std::string FIND_BY_DEVICE_AND_ADDRESS = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ? AND address = ?
    )";
    
    // 🔥 누락된 쿼리 3: FIND_WRITABLE_POINTS
    const std::string FIND_WRITABLE_POINTS = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE access_mode IN ('write', 'readwrite') AND is_enabled = 1
        ORDER BY device_id, address
    )";
    
    // 🔥 누락된 쿼리 4: FIND_BY_TAG
    const std::string FIND_BY_TAG = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE tags LIKE ?
        ORDER BY device_id, address
    )";
    
    // 🔥 누락된 쿼리 5: FIND_DISABLED
    const std::string FIND_DISABLED = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE is_enabled = 0
        ORDER BY device_id, address
    )";
    
    // 🔥 누락된 쿼리 6: FIND_LAST_CREATED_BY_DEVICE_ADDRESS
    const std::string FIND_LAST_CREATED_BY_DEVICE_ADDRESS = R"(
        SELECT 
            id, device_id, name, description, address, data_type, access_mode,
            is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
            log_enabled, log_interval_ms, log_deadband, tags, metadata,
            created_at, updated_at
        FROM data_points 
        WHERE device_id = ? AND address = ?
        ORDER BY created_at DESC
        LIMIT 1
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
    
    // 🔥 누락된 쿼리 7: EXISTS_BY_ID
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM data_points WHERE id = ?";
    
    // 🔥 누락된 쿼리 8: COUNT_ALL
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM data_points";
    
    const std::string COUNT_BY_DEVICE = "SELECT COUNT(*) as count FROM data_points WHERE device_id = ?";
    
    // 🔥 누락된 쿼리 9: GET_COUNT_BY_DEVICE (별칭)
    const std::string GET_COUNT_BY_DEVICE = R"(
        SELECT device_id, COUNT(*) as count 
        FROM data_points 
        GROUP BY device_id
        ORDER BY device_id
    )";
    
    // 🔥 누락된 쿼리 10: GET_COUNT_BY_DATA_TYPE
    const std::string GET_COUNT_BY_DATA_TYPE = R"(
        SELECT data_type, COUNT(*) as count 
        FROM data_points 
        GROUP BY data_type
        ORDER BY count DESC
    )";
    
    // 🔥 누락된 쿼리 11: GET_LAST_INSERT_ID
    const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";
    
    // 🔥 누락된 쿼리 12: CREATE_TABLE
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS data_points (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id INTEGER NOT NULL,
            name VARCHAR(100) NOT NULL,
            description TEXT,
            address INTEGER NOT NULL,
            data_type VARCHAR(20) NOT NULL,
            access_mode VARCHAR(10) DEFAULT 'read',
            is_enabled BOOLEAN DEFAULT true,
            unit VARCHAR(20),
            scaling_factor DECIMAL(10,6) DEFAULT 1.0,
            scaling_offset DECIMAL(10,6) DEFAULT 0.0,
            min_value DECIMAL(15,6),
            max_value DECIMAL(15,6),
            log_enabled BOOLEAN DEFAULT true,
            log_interval_ms INTEGER DEFAULT 0,
            log_deadband DECIMAL(10,6) DEFAULT 0.0,
            tags TEXT,
            metadata TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
            UNIQUE(device_id, address)
        )
    )";
    
} // namespace DataPoint

// =============================================================================
// 🎯 DeviceSettingsRepository 쿼리들 (완전판)
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
    
    const std::string FIND_BY_PROTOCOL = R"(
        SELECT 
            ds.device_id, ds.polling_interval_ms, ds.connection_timeout_ms, ds.max_retry_count,
            ds.retry_interval_ms, ds.backoff_time_ms, ds.keep_alive_enabled, ds.keep_alive_interval_s,
            ds.scan_rate_override, ds.read_timeout_ms, ds.write_timeout_ms, ds.backoff_multiplier,
            ds.max_backoff_time_ms, ds.keep_alive_timeout_s, ds.data_validation_enabled,
            ds.performance_monitoring_enabled, ds.diagnostic_mode_enabled, ds.updated_at
        FROM device_settings ds
        INNER JOIN devices d ON ds.device_id = d.id
        WHERE d.protocol_type = ?
        ORDER BY ds.device_id
    )";
    
    const std::string FIND_ACTIVE_DEVICES = R"(
        SELECT 
            ds.device_id, ds.polling_interval_ms, ds.connection_timeout_ms, ds.max_retry_count,
            ds.retry_interval_ms, ds.backoff_time_ms, ds.keep_alive_enabled, ds.keep_alive_interval_s,
            ds.scan_rate_override, ds.read_timeout_ms, ds.write_timeout_ms, ds.backoff_multiplier,
            ds.max_backoff_time_ms, ds.keep_alive_timeout_s, ds.data_validation_enabled,
            ds.performance_monitoring_enabled, ds.diagnostic_mode_enabled, ds.updated_at
        FROM device_settings ds
        INNER JOIN devices d ON ds.device_id = d.id
        WHERE d.is_enabled = 1
        ORDER BY ds.polling_interval_ms, ds.device_id
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
    
    const std::string UPDATE_POLLING_INTERVAL = R"(
        UPDATE device_settings 
        SET polling_interval_ms = ?, updated_at = ?
        WHERE device_id = ?
    )";
    
    const std::string UPDATE_CONNECTION_TIMEOUT = R"(
        UPDATE device_settings 
        SET connection_timeout_ms = ?, updated_at = ?
        WHERE device_id = ?
    )";
    
    const std::string UPDATE_RETRY_SETTINGS = R"(
        UPDATE device_settings 
        SET max_retry_count = ?, retry_interval_ms = ?, updated_at = ?
        WHERE device_id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM device_settings WHERE device_id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM device_settings WHERE device_id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM device_settings";
    
    const std::string GET_POLLING_INTERVAL_DISTRIBUTION = R"(
        SELECT polling_interval_ms, COUNT(*) as count 
        FROM device_settings 
        GROUP BY polling_interval_ms
        ORDER BY polling_interval_ms
    )";
    
    // 테이블 생성
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS device_settings (
            device_id INTEGER PRIMARY KEY,
            
            -- 기본 통신 설정
            polling_interval_ms INTEGER DEFAULT 1000,
            connection_timeout_ms INTEGER DEFAULT 10000,
            max_retry_count INTEGER DEFAULT 3,
            retry_interval_ms INTEGER DEFAULT 5000,
            backoff_time_ms INTEGER DEFAULT 60000,
            keep_alive_enabled BOOLEAN DEFAULT true,
            keep_alive_interval_s INTEGER DEFAULT 30,
            
            -- 고급 설정
            scan_rate_override INTEGER,
            read_timeout_ms INTEGER DEFAULT 5000,
            write_timeout_ms INTEGER DEFAULT 5000,
            backoff_multiplier DECIMAL(3,2) DEFAULT 1.5,
            max_backoff_time_ms INTEGER DEFAULT 300000,
            keep_alive_timeout_s INTEGER DEFAULT 10,
            data_validation_enabled BOOLEAN DEFAULT true,
            performance_monitoring_enabled BOOLEAN DEFAULT true,
            diagnostic_mode_enabled BOOLEAN DEFAULT false,
            
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
        )
    )";
    
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