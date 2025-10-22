/**
 * @file ExportSQLQueries.h
 * @brief Export 시스템 SQL 쿼리 중앙 관리 (리팩토링 v2.0.1)
 * @author PulseOne Development Team
 * @date 2025-10-21
 * @version 2.0.1
 * 
 * 저장 위치: core/shared/include/Database/ExportSQLQueries.h
 * 참조: 10-export_system.sql
 * 
 * 🔧 긴급 수정 (v2.0.0 → v2.0.1):
 *   - ExportTargetMapping::EXISTS_BY_ID 추가 (컴파일 에러 수정)
 * 
 * 주요 변경사항 (v1.0 → v2.0):
 *   - export_targets: 통계 필드 제거 (설정만 보관)
 *   - export_logs: 확장 및 통계 집계 쿼리 추가
 *   - VIEW를 통한 실시간 통계 제공
 */

#ifndef EXPORT_SQL_QUERIES_H
#define EXPORT_SQL_QUERIES_H

#include <string>

namespace PulseOne {
namespace Database {
namespace SQL {

// =============================================================================
// 🎯 ExportProfile 쿼리들 (export_profiles 테이블)
// =============================================================================
namespace ExportProfile {
    
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS export_profiles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name VARCHAR(100) NOT NULL UNIQUE,
            description TEXT,
            is_enabled BOOLEAN DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            created_by VARCHAR(50),
            point_count INTEGER DEFAULT 0,
            last_exported_at DATETIME
        )
    )";
    
    const std::string CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_profiles_enabled ON export_profiles(is_enabled);
        CREATE INDEX IF NOT EXISTS idx_profiles_created ON export_profiles(created_at DESC);
    )";
    
    const std::string FIND_ALL = R"(
        SELECT id, name, description, is_enabled, created_at, updated_at, 
               created_by, point_count, last_exported_at
        FROM export_profiles
        ORDER BY name ASC
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT id, name, description, is_enabled, created_at, updated_at, 
               created_by, point_count, last_exported_at
        FROM export_profiles WHERE id = ?
    )";
    
    const std::string INSERT = R"(
        INSERT INTO export_profiles (name, description, is_enabled, created_by)
        VALUES (?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE export_profiles SET
            name = ?, description = ?, is_enabled = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM export_profiles WHERE id = ?";
    
} // namespace ExportProfile

// =============================================================================
// 🎯 ExportTarget 쿼리들 (export_targets 테이블)
// 변경: 통계 필드 완전 제거, 설정만 보관
// =============================================================================
namespace ExportTarget {
    
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
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL
        )
    )";
    
    const std::string CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_export_targets_type ON export_targets(target_type);
        CREATE INDEX IF NOT EXISTS idx_export_targets_profile ON export_targets(profile_id);
        CREATE INDEX IF NOT EXISTS idx_export_targets_enabled ON export_targets(is_enabled);
        CREATE INDEX IF NOT EXISTS idx_export_targets_name ON export_targets(name);
    )";
    
    // 기본 CRUD (통계 필드 제거됨)
    const std::string FIND_ALL = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets
        ORDER BY name ASC
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE id = ?
    )";
    
    const std::string FIND_BY_NAME = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE name = ?
    )";
    
    const std::string FIND_BY_ENABLED = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE is_enabled = ? ORDER BY name ASC
    )";
    
    const std::string FIND_BY_TARGET_TYPE = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE target_type = ? ORDER BY name ASC
    )";
    
    const std::string FIND_BY_PROFILE_ID = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE profile_id = ? ORDER BY name ASC
    )";
    
    const std::string INSERT = R"(
        INSERT INTO export_targets (
            profile_id, name, target_type, description, is_enabled, config,
            export_mode, export_interval, batch_size
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE export_targets SET
            profile_id = ?, name = ?, target_type = ?, description = ?, is_enabled = ?,
            config = ?, export_mode = ?, export_interval = ?, batch_size = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM export_targets WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM export_targets WHERE id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM export_targets";
    
    const std::string COUNT_ACTIVE = "SELECT COUNT(*) as count FROM export_targets WHERE is_enabled = 1";
    
    const std::string COUNT_BY_TYPE = R"(
        SELECT target_type, COUNT(*) as count 
        FROM export_targets 
        GROUP BY target_type
    )";
    
} // namespace ExportTarget

// =============================================================================
// 🎯 ExportTargetMapping 쿼리들 (export_target_mappings 테이블)
// 🔧 v2.0.1: EXISTS_BY_ID 추가됨!
// =============================================================================
namespace ExportTargetMapping {
    
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
    
    const std::string CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_export_target_mappings_target ON export_target_mappings(target_id);
        CREATE INDEX IF NOT EXISTS idx_export_target_mappings_point ON export_target_mappings(point_id);
    )";
    
    const std::string FIND_ALL = R"(
        SELECT id, target_id, point_id, target_field_name, target_description,
               conversion_config, is_enabled, created_at
        FROM export_target_mappings
        ORDER BY target_id, point_id
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT id, target_id, point_id, target_field_name, target_description,
               conversion_config, is_enabled, created_at
        FROM export_target_mappings WHERE id = ?
    )";
    
    const std::string FIND_BY_TARGET_ID = R"(
        SELECT id, target_id, point_id, target_field_name, target_description,
               conversion_config, is_enabled, created_at
        FROM export_target_mappings
        WHERE target_id = ? ORDER BY point_id
    )";
    
    const std::string FIND_ENABLED_BY_TARGET_ID = R"(
        SELECT id, target_id, point_id, target_field_name, target_description,
               conversion_config, is_enabled, created_at
        FROM export_target_mappings
        WHERE target_id = ? AND is_enabled = 1
        ORDER BY point_id
    )";
    
    const std::string FIND_BY_POINT_ID = R"(
        SELECT id, target_id, point_id, target_field_name, target_description,
               conversion_config, is_enabled, created_at
        FROM export_target_mappings
        WHERE point_id = ? ORDER BY target_id
    )";
    
    const std::string FIND_BY_TARGET_AND_POINT = R"(
        SELECT id, target_id, point_id, target_field_name, target_description,
               conversion_config, is_enabled, created_at
        FROM export_target_mappings
        WHERE target_id = ? AND point_id = ?
    )";
    
    const std::string INSERT = R"(
        INSERT INTO export_target_mappings (
            target_id, point_id, target_field_name, target_description,
            conversion_config, is_enabled
        ) VALUES (?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE export_target_mappings SET
            target_id = ?, point_id = ?, target_field_name = ?, target_description = ?,
            conversion_config = ?, is_enabled = ?
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM export_target_mappings WHERE id = ?";
    
    const std::string DELETE_BY_TARGET_ID = "DELETE FROM export_target_mappings WHERE target_id = ?";
    
    const std::string DELETE_BY_POINT_ID = "DELETE FROM export_target_mappings WHERE point_id = ?";
    
    // 🔧 v2.0.1: 컴파일 에러 수정을 위해 추가
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM export_target_mappings WHERE id = ?";
    
    const std::string COUNT_BY_TARGET_ID = "SELECT COUNT(*) as count FROM export_target_mappings WHERE target_id = ?";
    
    const std::string COUNT_BY_POINT_ID = "SELECT COUNT(*) as count FROM export_target_mappings WHERE point_id = ?";
    
} // namespace ExportTargetMapping

// =============================================================================
// 🎯 ExportLog 쿼리들 (export_logs 테이블)
// 변경: 확장 및 통계 집계 쿼리 추가
// =============================================================================
namespace ExportLog {
    
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
    
    const std::string CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_export_logs_type ON export_logs(log_type);
        CREATE INDEX IF NOT EXISTS idx_export_logs_timestamp ON export_logs(timestamp DESC);
        CREATE INDEX IF NOT EXISTS idx_export_logs_status ON export_logs(status);
        CREATE INDEX IF NOT EXISTS idx_export_logs_target ON export_logs(target_id);
        CREATE INDEX IF NOT EXISTS idx_export_logs_target_time ON export_logs(target_id, timestamp DESC);
    )";
    
    // 기본 CRUD
    const std::string FIND_ALL = R"(
        SELECT id, log_type, service_id, target_id, mapping_id, point_id,
               source_value, converted_value, status, error_message, error_code,
               response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        ORDER BY timestamp DESC LIMIT 1000
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT id, log_type, service_id, target_id, mapping_id, point_id,
               source_value, converted_value, status, error_message, error_code,
               response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs WHERE id = ?
    )";
    
    const std::string FIND_BY_TIME_RANGE = R"(
        SELECT id, log_type, service_id, target_id, mapping_id, point_id,
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
            log_type = ?, service_id = ?, target_id = ?, mapping_id = ?, point_id = ?,
            source_value = ?, converted_value = ?, status = ?, error_message = ?,
            error_code = ?, response_data = ?, http_status_code = ?,
            processing_time_ms = ?, client_info = ?
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM export_logs WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM export_logs WHERE id = ?";
    
    // 전용 조회
    const std::string FIND_BY_TARGET_ID = R"(
        SELECT id, log_type, service_id, target_id, mapping_id, point_id,
               source_value, converted_value, status, error_message, error_code,
               response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE target_id = ?
        ORDER BY timestamp DESC LIMIT ?
    )";
    
    const std::string FIND_BY_STATUS = R"(
        SELECT id, log_type, service_id, target_id, mapping_id, point_id,
               source_value, converted_value, status, error_message, error_code,
               response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE status = ?
        ORDER BY timestamp DESC LIMIT ?
    )";
    
    const std::string FIND_RECENT = R"(
        SELECT id, log_type, service_id, target_id, mapping_id, point_id,
               source_value, converted_value, status, error_message, error_code,
               response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' hours')
        ORDER BY timestamp DESC LIMIT ?
    )";
    
    const std::string FIND_RECENT_FAILURES = R"(
        SELECT id, log_type, service_id, target_id, mapping_id, point_id,
               source_value, converted_value, status, error_message, error_code,
               response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE status = 'failed' AND timestamp >= datetime('now', '-' || ? || ' hours')
        ORDER BY timestamp DESC LIMIT ?
    )";
    
    const std::string FIND_BY_POINT_ID = R"(
        SELECT id, log_type, service_id, target_id, mapping_id, point_id,
               source_value, converted_value, status, error_message, error_code,
               response_data, http_status_code, processing_time_ms, timestamp, client_info
        FROM export_logs
        WHERE point_id = ?
        ORDER BY timestamp DESC LIMIT ?
    )";
    
    // ========================================================================
    // 🆕 통계 집계 쿼리들 (export_targets 통계 필드 대체)
    // ========================================================================
    
    // Target별 전체 통계
    const std::string GET_TARGET_STATISTICS_ALL = R"(
        SELECT 
            COUNT(*) as total_exports,
            SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as successful_exports,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed_exports,
            AVG(CASE WHEN status = 'success' THEN processing_time_ms END) as avg_export_time_ms,
            MAX(timestamp) as last_export_at,
            MAX(CASE WHEN status = 'success' THEN timestamp END) as last_success_at,
            MAX(CASE WHEN status = 'failed' THEN timestamp END) as last_error_at
        FROM export_logs
        WHERE target_id = ?
    )";
    
    // Target별 시간대별 통계 (최근 N시간)
    const std::string GET_TARGET_STATISTICS_RECENT = R"(
        SELECT 
            COUNT(*) as total_exports,
            SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as successful_exports,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed_exports,
            AVG(CASE WHEN status = 'success' THEN processing_time_ms END) as avg_export_time_ms,
            MAX(timestamp) as last_export_at,
            MAX(CASE WHEN status = 'success' THEN timestamp END) as last_success_at,
            MAX(CASE WHEN status = 'failed' THEN timestamp END) as last_error_at,
            (SELECT error_message FROM export_logs 
             WHERE target_id = ? AND status = 'failed' 
             ORDER BY timestamp DESC LIMIT 1) as last_error
        FROM export_logs
        WHERE target_id = ? AND timestamp >= datetime('now', '-' || ? || ' hours')
    )";
    
    // 전체 시스템 통계
    const std::string GET_OVERALL_STATISTICS = R"(
        SELECT 
            COUNT(*) as total_exports,
            SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as successful_exports,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed_exports,
            AVG(CASE WHEN status = 'success' THEN processing_time_ms END) as avg_export_time_ms
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' hours')
    )";
    
    // Target별 상태 분포
    const std::string GET_STATUS_DISTRIBUTION = R"(
        SELECT status, COUNT(*) as count
        FROM export_logs
        WHERE target_id = ? AND timestamp >= datetime('now', '-' || ? || ' hours')
        GROUP BY status
    )";
    
    // 평균 처리시간 (성공 건만)
    const std::string GET_AVERAGE_PROCESSING_TIME = R"(
        SELECT AVG(processing_time_ms) as avg_time
        FROM export_logs
        WHERE target_id = ? AND status = 'success' AND timestamp >= datetime('now', '-' || ? || ' hours')
    )";
    
    // 최근 에러 목록
    const std::string GET_RECENT_ERRORS = R"(
        SELECT error_message, error_code, COUNT(*) as count, MAX(timestamp) as last_occurred
        FROM export_logs
        WHERE target_id = ? AND status = 'failed' AND timestamp >= datetime('now', '-' || ? || ' hours')
        GROUP BY error_message, error_code
        ORDER BY count DESC LIMIT ?
    )";
    
    // ========================================================================
    // 정리 (Log Retention)
    // ========================================================================
    
    const std::string DELETE_OLDER_THAN = R"(
        DELETE FROM export_logs
        WHERE timestamp < datetime('now', '-' || ? || ' days')
    )";
    
    const std::string DELETE_SUCCESS_LOGS_OLDER_THAN = R"(
        DELETE FROM export_logs
        WHERE status = 'success' AND timestamp < datetime('now', '-' || ? || ' days')
    )";
    
    const std::string COUNT_LOGS_OLDER_THAN = R"(
        SELECT COUNT(*) as count
        FROM export_logs
        WHERE timestamp < datetime('now', '-' || ? || ' days')
    )";
    
    
    // ========================================================================
    // 🆕 고급 통계 쿼리들 (v2.1.0 추가)
    // ========================================================================
    
    const std::string GET_HOURLY_STATISTICS_ALL = R"(
        SELECT 
            strftime('%Y-%m-%d %H:00', timestamp) as time_label,
            COUNT(*) as total_count,
            SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as success_count,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed_count,
            CAST(SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) AS REAL) 
                / COUNT(*) * 100.0 as success_rate,
            AVG(CASE WHEN status = 'success' THEN processing_time_ms END) 
                as avg_processing_time_ms
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' hours')
        GROUP BY strftime('%Y-%m-%d %H:00', timestamp)
        ORDER BY time_label DESC
    )";
    
    const std::string GET_HOURLY_STATISTICS_BY_TARGET = R"(
        SELECT 
            strftime('%Y-%m-%d %H:00', timestamp) as time_label,
            COUNT(*) as total_count,
            SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as success_count,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed_count,
            CAST(SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) AS REAL) 
                / COUNT(*) * 100.0 as success_rate,
            AVG(CASE WHEN status = 'success' THEN processing_time_ms END) 
                as avg_processing_time_ms
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' hours')
          AND target_id = ?
        GROUP BY strftime('%Y-%m-%d %H:00', timestamp)
        ORDER BY time_label DESC
    )";
    
    const std::string GET_DAILY_STATISTICS_ALL = R"(
        SELECT 
            strftime('%Y-%m-%d', timestamp) as time_label,
            COUNT(*) as total_count,
            SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as success_count,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed_count,
            CAST(SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) AS REAL) 
                / COUNT(*) * 100.0 as success_rate,
            AVG(CASE WHEN status = 'success' THEN processing_time_ms END) 
                as avg_processing_time_ms
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' days')
        GROUP BY strftime('%Y-%m-%d', timestamp)
        ORDER BY time_label DESC
    )";
    
    const std::string GET_DAILY_STATISTICS_BY_TARGET = R"(
        SELECT 
            strftime('%Y-%m-%d', timestamp) as time_label,
            COUNT(*) as total_count,
            SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as success_count,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed_count,
            CAST(SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) AS REAL) 
                / COUNT(*) * 100.0 as success_rate,
            AVG(CASE WHEN status = 'success' THEN processing_time_ms END) 
                as avg_processing_time_ms
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' days')
          AND target_id = ?
        GROUP BY strftime('%Y-%m-%d', timestamp)
        ORDER BY time_label DESC
    )";
    
    const std::string GET_ERROR_TYPE_STATISTICS_ALL = R"(
        SELECT 
            error_code,
            error_message,
            COUNT(*) as occurrence_count,
            MIN(timestamp) as first_occurred,
            MAX(timestamp) as last_occurred
        FROM export_logs
        WHERE status = 'failed'
          AND timestamp >= datetime('now', '-' || ? || ' hours')
        GROUP BY error_code, error_message
        ORDER BY occurrence_count DESC
        LIMIT ?
    )";
    
    const std::string GET_ERROR_TYPE_STATISTICS_BY_TARGET = R"(
        SELECT 
            error_code,
            error_message,
            COUNT(*) as occurrence_count,
            MIN(timestamp) as first_occurred,
            MAX(timestamp) as last_occurred
        FROM export_logs
        WHERE status = 'failed'
          AND timestamp >= datetime('now', '-' || ? || ' hours')
          AND target_id = ?
        GROUP BY error_code, error_message
        ORDER BY occurrence_count DESC
        LIMIT ?
    )";
    
    const std::string GET_POINT_PERFORMANCE_STATS_ALL = R"(
        SELECT 
            point_id,
            COUNT(*) as total_exports,
            SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as successful_exports,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed_exports,
            CAST(SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) AS REAL) 
                / COUNT(*) * 100.0 as success_rate,
            AVG(CASE WHEN status = 'success' THEN processing_time_ms END) 
                as avg_processing_time_ms,
            MAX(timestamp) as last_export_time
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' hours')
        GROUP BY point_id
        ORDER BY total_exports DESC
        LIMIT ?
    )";
    
    const std::string GET_POINT_PERFORMANCE_STATS_BY_TARGET = R"(
        SELECT 
            point_id,
            COUNT(*) as total_exports,
            SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as successful_exports,
            SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END) as failed_exports,
            CAST(SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) AS REAL) 
                / COUNT(*) * 100.0 as success_rate,
            AVG(CASE WHEN status = 'success' THEN processing_time_ms END) 
                as avg_processing_time_ms,
            MAX(timestamp) as last_export_time
        FROM export_logs
        WHERE timestamp >= datetime('now', '-' || ? || ' hours')
          AND target_id = ?
        GROUP BY point_id
        ORDER BY total_exports DESC
        LIMIT ?
    )";
    
    const std::string GET_TARGET_HEALTH_CHECK = R"(
        SELECT 
            ? as target_id,
            CAST(SUM(CASE WHEN status = 'success' 
                AND timestamp >= datetime('now', '-1 hours') 
                THEN 1 ELSE 0 END) AS REAL) / 
            NULLIF(SUM(CASE WHEN timestamp >= datetime('now', '-1 hours') 
                THEN 1 ELSE 0 END), 0) * 100.0 as success_rate_1h,
            CAST(SUM(CASE WHEN status = 'success' 
                AND timestamp >= datetime('now', '-24 hours') 
                THEN 1 ELSE 0 END) AS REAL) / 
            NULLIF(SUM(CASE WHEN timestamp >= datetime('now', '-24 hours') 
                THEN 1 ELSE 0 END), 0) * 100.0 as success_rate_24h,
            AVG(CASE WHEN status = 'success' 
                AND timestamp >= datetime('now', '-1 hours')
                THEN processing_time_ms END) as avg_response_time_ms,
            MAX(CASE WHEN status = 'success' THEN timestamp END) as last_success_time,
            MAX(CASE WHEN status = 'failed' THEN timestamp END) as last_failure_time,
            (SELECT error_message FROM export_logs 
             WHERE target_id = ?
               AND status = 'failed'
             ORDER BY timestamp DESC LIMIT 1) as last_error_message
        FROM export_logs
        WHERE target_id = ?
    )";
    
    const std::string GET_CONSECUTIVE_FAILURES = R"(
        SELECT status
        FROM export_logs
        WHERE target_id = ?
        ORDER BY timestamp DESC
        LIMIT 100
    )";
    
    const std::string GET_DISTINCT_TARGET_IDS = R"(
        SELECT DISTINCT target_id 
        FROM export_logs 
        WHERE timestamp >= datetime('now', '-24 hours')
        ORDER BY target_id
    )";
} // namespace ExportLog

// =============================================================================
// 🎯 ExportTargetStats VIEW (실시간 통계 뷰)
// =============================================================================
namespace ExportTargetStatsView {
    
    const std::string CREATE_VIEW = R"(
        CREATE VIEW IF NOT EXISTS export_target_stats AS
        SELECT 
            t.id,
            t.name,
            t.target_type,
            t.is_enabled,
            
            -- 최근 24시간 통계
            COUNT(l.id) as total_exports_24h,
            SUM(CASE WHEN l.status = 'success' THEN 1 ELSE 0 END) as successful_exports_24h,
            SUM(CASE WHEN l.status = 'failed' THEN 1 ELSE 0 END) as failed_exports_24h,
            AVG(CASE WHEN l.status = 'success' THEN l.processing_time_ms END) as avg_time_ms_24h,
            
            -- 전체 통계
            (SELECT COUNT(*) FROM export_logs WHERE target_id = t.id) as total_exports_all,
            (SELECT COUNT(*) FROM export_logs WHERE target_id = t.id AND status = 'success') as successful_exports_all,
            (SELECT COUNT(*) FROM export_logs WHERE target_id = t.id AND status = 'failed') as failed_exports_all,
            
            -- 마지막 정보
            MAX(CASE WHEN l.status = 'success' THEN l.timestamp END) as last_success_at,
            MAX(CASE WHEN l.status = 'failed' THEN l.timestamp END) as last_failure_at,
            (SELECT error_message FROM export_logs 
             WHERE target_id = t.id AND status = 'failed' 
             ORDER BY timestamp DESC LIMIT 1) as last_error
             
        FROM export_targets t
        LEFT JOIN export_logs l ON t.id = l.target_id 
            AND l.timestamp > datetime('now', '-24 hours')
        GROUP BY t.id
    )";
    
    const std::string DROP_VIEW = "DROP VIEW IF EXISTS export_target_stats";
    
    const std::string SELECT_ALL = R"(
        SELECT * FROM export_target_stats ORDER BY name ASC
    )";
    
    const std::string SELECT_BY_ID = R"(
        SELECT * FROM export_target_stats WHERE id = ?
    )";
    
    const std::string SELECT_ACTIVE = R"(
        SELECT * FROM export_target_stats WHERE is_enabled = 1 ORDER BY name ASC
    )";
    
} // namespace ExportTargetStatsView

} // namespace SQL
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_SQL_QUERIES_H