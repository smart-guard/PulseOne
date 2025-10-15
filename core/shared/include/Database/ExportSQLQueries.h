/**
 * @file ExportSQLQueries.h
 * @brief Export 시스템 SQL 쿼리 중앙 관리
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.0
 * 저장 위치: core/shared/include/Database/ExportSQLQueries.h
 * 
 * 참조: 001_export_system_schema.sql
 */

#ifndef EXPORT_SQL_QUERIES_H
#define EXPORT_SQL_QUERIES_H

#include <string>

namespace PulseOne {
namespace Database {
namespace SQL {

// =============================================================================
// 🎯 ExportTarget 쿼리들 (export_targets 테이블)
// =============================================================================
namespace ExportTarget {
    
    // 테이블 생성
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS export_targets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            profile_id INTEGER,
            name VARCHAR(100) NOT NULL UNIQUE,
            target_type VARCHAR(20) NOT NULL,
            description TEXT,
            is_enabled BOOLEAN DEFAULT 1,
            config TEXT NOT NULL,
            export_mode VARCHAR(20) DEFAULT 'on_change',
            export_interval INTEGER DEFAULT 0,
            batch_size INTEGER DEFAULT 100,
            total_exports INTEGER DEFAULT 0,
            successful_exports INTEGER DEFAULT 0,
            failed_exports INTEGER DEFAULT 0,
            last_export_at DATETIME,
            last_success_at DATETIME,
            last_error TEXT,
            last_error_at DATETIME,
            avg_export_time_ms INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL
        )
    )";
    
    // 기본 CRUD
    const std::string FIND_ALL = R"(
        SELECT 
            id, profile_id, name, target_type, description, is_enabled, config,
            export_mode, export_interval, batch_size, total_exports, 
            successful_exports, failed_exports, last_export_at, last_success_at,
            last_error, last_error_at, avg_export_time_ms, created_at, updated_at
        FROM export_targets
        ORDER BY name ASC
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, profile_id, name, target_type, description, is_enabled, config,
            export_mode, export_interval, batch_size, total_exports, 
            successful_exports, failed_exports, last_export_at, last_success_at,
            last_error, last_error_at, avg_export_time_ms, created_at, updated_at
        FROM export_targets 
        WHERE id = ?
    )";
    
    const std::string INSERT = R"(
        INSERT INTO export_targets (
            profile_id, name, target_type, description, is_enabled, config,
            export_mode, export_interval, batch_size
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE export_targets SET
            profile_id = ?,
            name = ?,
            target_type = ?,
            description = ?,
            is_enabled = ?,
            config = ?,
            export_mode = ?,
            export_interval = ?,
            batch_size = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM export_targets WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM export_targets WHERE id = ?";
    
    // 전용 조회
    const std::string FIND_BY_ENABLED = R"(
        SELECT 
            id, profile_id, name, target_type, description, is_enabled, config,
            export_mode, export_interval, batch_size, total_exports, 
            successful_exports, failed_exports, last_export_at, last_success_at,
            last_error, last_error_at, avg_export_time_ms, created_at, updated_at
        FROM export_targets
        WHERE is_enabled = ?
        ORDER BY name ASC
    )";
    
    const std::string FIND_BY_TARGET_TYPE = R"(
        SELECT 
            id, profile_id, name, target_type, description, is_enabled, config,
            export_mode, export_interval, batch_size, total_exports, 
            successful_exports, failed_exports, last_export_at, last_success_at,
            last_error, last_error_at, avg_export_time_ms, created_at, updated_at
        FROM export_targets
        WHERE target_type = ?
        ORDER BY name ASC
    )";
    
    const std::string FIND_BY_PROFILE_ID = R"(
        SELECT 
            id, profile_id, name, target_type, description, is_enabled, config,
            export_mode, export_interval, batch_size, total_exports, 
            successful_exports, failed_exports, last_export_at, last_success_at,
            last_error, last_error_at, avg_export_time_ms, created_at, updated_at
        FROM export_targets
        WHERE profile_id = ?
        ORDER BY name ASC
    )";
    
    const std::string FIND_BY_NAME = R"(
        SELECT 
            id, profile_id, name, target_type, description, is_enabled, config,
            export_mode, export_interval, batch_size, total_exports, 
            successful_exports, failed_exports, last_export_at, last_success_at,
            last_error, last_error_at, avg_export_time_ms, created_at, updated_at
        FROM export_targets
        WHERE name = ?
    )";
    
    // 통계 업데이트
    const std::string UPDATE_STATISTICS = R"(
        UPDATE export_targets SET
            total_exports = total_exports + 1,
            successful_exports = successful_exports + ?,
            failed_exports = failed_exports + ?,
            last_export_at = CURRENT_TIMESTAMP,
            last_success_at = CASE WHEN ? = 1 THEN CURRENT_TIMESTAMP ELSE last_success_at END,
            last_error = ?,
            last_error_at = CASE WHEN ? = 0 THEN CURRENT_TIMESTAMP ELSE last_error_at END,
            avg_export_time_ms = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // 카운트
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM export_targets";
    
    const std::string COUNT_ACTIVE = "SELECT COUNT(*) as count FROM export_targets WHERE is_enabled = 1";
    
    const std::string COUNT_BY_TYPE = "SELECT target_type, COUNT(*) as count FROM export_targets GROUP BY target_type";
    
} // namespace ExportTarget

// =============================================================================
// 🎯 ExportTargetMapping 쿼리들 (export_target_mappings 테이블)
// =============================================================================
namespace ExportTargetMapping {
    
    // 테이블 생성
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS export_target_mappings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            target_id INTEGER NOT NULL,
            point_id INTEGER NOT NULL,
            target_field_name VARCHAR(200),
            target_description VARCHAR(500),
            conversion_config TEXT,
            is_enabled BOOLEAN DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE CASCADE,
            FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE,
            UNIQUE(target_id, point_id)
        )
    )";
    
    // 기본 CRUD
    const std::string FIND_ALL = R"(
        SELECT 
            id, target_id, point_id, target_field_name, target_description,
            conversion_config, is_enabled, created_at
        FROM export_target_mappings
        ORDER BY target_id, point_id
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, target_id, point_id, target_field_name, target_description,
            conversion_config, is_enabled, created_at
        FROM export_target_mappings
        WHERE id = ?
    )";
    
    const std::string INSERT = R"(
        INSERT INTO export_target_mappings (
            target_id, point_id, target_field_name, target_description,
            conversion_config, is_enabled
        ) VALUES (?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE export_target_mappings SET
            target_id = ?,
            point_id = ?,
            target_field_name = ?,
            target_description = ?,
            conversion_config = ?,
            is_enabled = ?
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM export_target_mappings WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM export_target_mappings WHERE id = ?";
    
    // 전용 조회
    const std::string FIND_BY_TARGET_ID = R"(
        SELECT 
            id, target_id, point_id, target_field_name, target_description,
            conversion_config, is_enabled, created_at
        FROM export_target_mappings
        WHERE target_id = ?
        ORDER BY point_id
    )";
    
    const std::string FIND_BY_POINT_ID = R"(
        SELECT 
            id, target_id, point_id, target_field_name, target_description,
            conversion_config, is_enabled, created_at
        FROM export_target_mappings
        WHERE point_id = ?
        ORDER BY target_id
    )";
    
    const std::string FIND_BY_TARGET_AND_POINT = R"(
        SELECT 
            id, target_id, point_id, target_field_name, target_description,
            conversion_config, is_enabled, created_at
        FROM export_target_mappings
        WHERE target_id = ? AND point_id = ?
    )";
    
    const std::string FIND_ENABLED_BY_TARGET_ID = R"(
        SELECT 
            id, target_id, point_id, target_field_name, target_description,
            conversion_config, is_enabled, created_at
        FROM export_target_mappings
        WHERE target_id = ? AND is_enabled = 1
        ORDER BY point_id
    )";
    
    const std::string DELETE_BY_TARGET_ID = "DELETE FROM export_target_mappings WHERE target_id = ?";
    
    const std::string DELETE_BY_POINT_ID = "DELETE FROM export_target_mappings WHERE point_id = ?";
    
    // 카운트
    const std::string COUNT_BY_TARGET_ID = "SELECT COUNT(*) as count FROM export_target_mappings WHERE target_id = ?";
    
    const std::string COUNT_BY_POINT_ID = "SELECT COUNT(*) as count FROM export_target_mappings WHERE point_id = ?";
    
} // namespace ExportTargetMapping

// =============================================================================
// 🎯 ExportLog 쿼리들 (export_logs 테이블)
// =============================================================================
namespace ExportLog {
    
    // 테이블 생성
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS export_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            log_type VARCHAR(20) NOT NULL,
            service_id INTEGER,
            target_id INTEGER,
            mapping_id INTEGER,
            point_id INTEGER,
            source_value TEXT,
            converted_value TEXT,
            status VARCHAR(20) NOT NULL,
            error_message TEXT,
            error_code VARCHAR(50),
            response_data TEXT,
            http_status_code INTEGER,
            processing_time_ms INTEGER,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            client_info TEXT,
            
            FOREIGN KEY (service_id) REFERENCES protocol_services(id) ON DELETE SET NULL,
            FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE SET NULL,
            FOREIGN KEY (mapping_id) REFERENCES protocol_mappings(id) ON DELETE SET NULL
        )
    )";
    
    // 인덱스 생성
    const std::string CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_export_logs_type ON export_logs(log_type);
        CREATE INDEX IF NOT EXISTS idx_export_logs_timestamp ON export_logs(timestamp DESC);
        CREATE INDEX IF NOT EXISTS idx_export_logs_status ON export_logs(status);
        CREATE INDEX IF NOT EXISTS idx_export_logs_target ON export_logs(target_id);
    )";
    
    // 기본 CRUD
    const std::string FIND_ALL = R"(
        SELECT 
            id, log_type, service_id, target_id, mapping_id, point_id,
            source_value, converted_value, status, error_message, error_code,
            response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        ORDER BY timestamp DESC
        LIMIT 1000
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, log_type, service_id, target_id, mapping_id, point_id,
            source_value, converted_value, status, error_message, error_code,
            response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE id = ?
    )";

    const std::string FIND_BY_TIME_RANGE = R"(
        SELECT 
            id, log_type, service_id, target_id, mapping_id, point_id,
            source_value, converted_value, status, error_message, error_code,
            response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE timestamp BETWEEN ? AND ?
        ORDER BY timestamp DESC
    )";

    const std::string INSERT = R"(
        INSERT INTO export_logs (
            log_type, service_id, target_id, mapping_id, point_id,
            source_value, converted_value, status, error_message, error_code,
            response_data, http_status_code, processing_time_ms, client_info
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    const std::string UPDATE = R"(
        UPDATE export_logs SET
            log_type = ?,
            service_id = ?,
            target_id = ?,
            mapping_id = ?,
            point_id = ?,
            source_value = ?,
            converted_value = ?,
            status = ?,
            error_message = ?,
            error_code = ?,
            response_data = ?,
            http_status_code = ?,
            processing_time_ms = ?,
            client_info = ?
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM export_logs WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM export_logs WHERE id = ?";
    
    // 전용 조회
    const std::string FIND_BY_TARGET_ID = R"(
        SELECT 
            id, log_type, service_id, target_id, mapping_id, point_id,
            source_value, converted_value, status, error_message, error_code,
            response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE target_id = ?
        ORDER BY timestamp DESC
        LIMIT ?
    )";
    
    const std::string FIND_BY_STATUS = R"(
        SELECT 
            id, log_type, service_id, target_id, mapping_id, point_id,
            source_value, converted_value, status, error_message, error_code,
            response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE status = ?
        ORDER BY timestamp DESC
        LIMIT ?
    )";
    
    const std::string FIND_RECENT = R"(
        SELECT 
            id, log_type, service_id, target_id, mapping_id, point_id,
            source_value, converted_value, status, error_message, error_code,
            response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' hours')
        ORDER BY timestamp DESC
        LIMIT ?
    )";
    
    const std::string FIND_RECENT_FAILURES = R"(
        SELECT 
            id, log_type, service_id, target_id, mapping_id, point_id,
            source_value, converted_value, status, error_message, error_code,
            response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE status = 'failed' 
          AND timestamp >= datetime('now', '-' || ? || ' hours')
        ORDER BY timestamp DESC
        LIMIT ?
    )";
    
    const std::string FIND_BY_POINT_ID = R"(
        SELECT 
            id, log_type, service_id, target_id, mapping_id, point_id,
            source_value, converted_value, status, error_message, error_code,
            response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE point_id = ?
        ORDER BY timestamp DESC
        LIMIT ?
    )";
    
    // 통계
    const std::string GET_TARGET_STATISTICS = R"(
        SELECT 
            status, COUNT(*) as count
        FROM export_logs
        WHERE target_id = ?
          AND timestamp >= datetime('now', '-' || ? || ' hours')
        GROUP BY status
    )";
    
    const std::string GET_OVERALL_STATISTICS = R"(
        SELECT 
            status, COUNT(*) as count
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' hours')
        GROUP BY status
    )";
    
    const std::string GET_AVERAGE_PROCESSING_TIME = R"(
        SELECT 
            AVG(processing_time_ms) as avg_time
        FROM export_logs
        WHERE target_id = ?
          AND status = 'success'
          AND timestamp >= datetime('now', '-' || ? || ' hours')
    )";
    
    // 정리
    const std::string DELETE_OLDER_THAN = R"(
        DELETE FROM export_logs
        WHERE timestamp < datetime('now', '-' || ? || ' days')
    )";
    
    const std::string DELETE_SUCCESS_LOGS_OLDER_THAN = R"(
        DELETE FROM export_logs
        WHERE status = 'success'
          AND timestamp < datetime('now', '-' || ? || ' days')
    )";
    
} // namespace ExportLog

} // namespace SQL
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_SQL_QUERIES_H