// =============================================================================
// collector/include/Database/ExtendedSQLQueries.h
// 🔧 완전 SQLite 호환 버전 - FOREIGN KEY 모두 제거
// =============================================================================

#ifndef EXTENDED_SQL_QUERIES_H
#define EXTENDED_SQL_QUERIES_H

#include <string>

namespace PulseOne {
namespace Database {
namespace SQL {

// =============================================================================
// AlarmOccurrence 관련 쿼리들 (FOREIGN KEY 제거됨)
// =============================================================================
namespace AlarmOccurrence {
    
    // 테이블 생성 (SQLite 호환 - FOREIGN KEY 없음)
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS alarm_occurrences (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_id INTEGER NOT NULL,
            tenant_id INTEGER NOT NULL,
            occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            trigger_value TEXT,
            trigger_condition TEXT,
            alarm_message TEXT,
            severity VARCHAR(20),
            state VARCHAR(20) DEFAULT 'active',
            acknowledged_time DATETIME,
            acknowledged_by INTEGER,
            acknowledge_comment TEXT,
            cleared_time DATETIME,
            cleared_value TEXT,
            clear_comment TEXT,
            notification_sent INTEGER DEFAULT 0,
            notification_time DATETIME,
            notification_count INTEGER DEFAULT 0,
            notification_result TEXT,
            context_data TEXT,
            source_name VARCHAR(100),
            location VARCHAR(200),
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    // 기본 CRUD 쿼리들
    const std::string FIND_ALL = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE id = ?
    )";
    
    const std::string INSERT = R"(
        INSERT INTO alarm_occurrences (
            rule_id, tenant_id, trigger_value, trigger_condition, alarm_message,
            severity, state, context_data, source_name, location
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE alarm_occurrences SET
            rule_id = ?, tenant_id = ?, trigger_value = ?, trigger_condition = ?,
            alarm_message = ?, severity = ?, state = ?, context_data = ?,
            source_name = ?, location = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM alarm_occurrences WHERE id = ?";
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE id = ?";
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM alarm_occurrences";
    const std::string FIND_MAX_ID = "SELECT COALESCE(MAX(id), 0) as max_id FROM alarm_occurrences";
    
    // 특화 조회 쿼리들
    const std::string FIND_ACTIVE = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE state = 'active'
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_BY_RULE_ID = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE rule_id = ?
        ORDER BY occurrence_time DESC
    )";
    
    // 알람 상태 관리 쿼리들
    const std::string ACKNOWLEDGE = R"(
        UPDATE alarm_occurrences SET
            state = 'acknowledged',
            acknowledged_time = CURRENT_TIMESTAMP,
            acknowledged_by = ?,
            acknowledge_comment = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string CLEAR = R"(
        UPDATE alarm_occurrences SET
            state = 'cleared',
            cleared_time = CURRENT_TIMESTAMP,
            cleared_value = ?,
            clear_comment = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // CLEAR_ALARM는 CLEAR의 별칭
    const std::string CLEAR_ALARM = CLEAR;
    
} // namespace AlarmOccurrence

// =============================================================================
// AlarmRule 관련 쿼리들 (FOREIGN KEY 제거됨)
// =============================================================================
namespace AlarmRule {
    
    // 테이블 생성 (SQLite 호환 - FOREIGN KEY 없음)
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS alarm_rules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            name VARCHAR(100) NOT NULL,
            description TEXT,
            target_type VARCHAR(20) NOT NULL,
            target_id INTEGER,
            target_group VARCHAR(100),
            alarm_type VARCHAR(20) NOT NULL,
            high_high_limit REAL,
            high_limit REAL,
            low_limit REAL,
            low_low_limit REAL,
            deadband REAL DEFAULT 0,
            rate_of_change REAL DEFAULT 0,
            trigger_condition VARCHAR(20),
            condition_script TEXT,
            message_script TEXT,
            message_config TEXT,
            message_template TEXT,
            severity VARCHAR(20) DEFAULT 'medium',
            priority INTEGER DEFAULT 100,
            auto_acknowledge INTEGER DEFAULT 0,
            acknowledge_timeout_min INTEGER DEFAULT 0,
            auto_clear INTEGER DEFAULT 1,
            suppression_rules TEXT,
            notification_enabled INTEGER DEFAULT 1,
            notification_delay_sec INTEGER DEFAULT 0,
            notification_repeat_interval_min INTEGER DEFAULT 0,
            notification_channels TEXT,
            notification_recipients TEXT,
            is_enabled INTEGER DEFAULT 1,
            is_latched INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            created_by INTEGER,
            FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
            FOREIGN KEY (created_by) REFERENCES users(id)
        )
    )";
    // 기본 CRUD 쿼리들
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        ORDER BY priority DESC, created_at DESC
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        WHERE id = ?
    )";
    
    const std::string FIND_BY_TARGET = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        WHERE target_type = ? AND target_id = ? AND is_enabled = 1
        ORDER BY priority DESC
    )";
    
    const std::string FIND_ENABLED = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by
        FROM alarm_rules 
        WHERE is_enabled = 1
        ORDER BY priority DESC, created_at DESC
    )";
    
    const std::string INSERT = R"(
        INSERT INTO alarm_rules (
            tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched, created_by
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE alarm_rules SET
            tenant_id = ?, name = ?, description = ?, target_type = ?, target_id = ?,
            target_group = ?, alarm_type = ?, high_high_limit = ?, high_limit = ?,
            low_limit = ?, low_low_limit = ?, deadband = ?, rate_of_change = ?,
            trigger_condition = ?, condition_script = ?, message_script = ?,
            message_config = ?, message_template = ?, severity = ?, priority = ?,
            auto_acknowledge = ?, acknowledge_timeout_min = ?, auto_clear = ?,
            suppression_rules = ?, notification_enabled = ?, notification_delay_sec = ?,
            notification_repeat_interval_min = ?, notification_channels = ?,
            notification_recipients = ?, is_enabled = ?, is_latched = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM alarm_rules WHERE id = ?";
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM alarm_rules WHERE id = ?";
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM alarm_rules";
    const std::string COUNT_ENABLED = "SELECT COUNT(*) as count FROM alarm_rules WHERE is_enabled = 1";
    
} // namespace AlarmRule

// =============================================================================
// ScriptLibrary 관련 쿼리들 (FOREIGN KEY 제거됨)
// =============================================================================
namespace ScriptLibrary {
    
    // 테이블 생성 (SQLite 호환 - FOREIGN KEY 없음)
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS script_library (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER DEFAULT 0,
            name VARCHAR(100) NOT NULL,
            display_name VARCHAR(255),
            description TEXT,
            category VARCHAR(50) DEFAULT 'CUSTOM',
            script_code TEXT NOT NULL,
            parameters TEXT DEFAULT '{}',
            return_type VARCHAR(20) DEFAULT 'REAL',
            tags TEXT DEFAULT '[]',
            example_usage TEXT,
            is_system INTEGER DEFAULT 0,
            is_template INTEGER DEFAULT 0,
            usage_count INTEGER DEFAULT 0,
            rating REAL DEFAULT 0.0,
            version VARCHAR(20) DEFAULT '1.0.0',
            author VARCHAR(100),
            license VARCHAR(50),
            dependencies TEXT DEFAULT '[]',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    // 기본 CRUD 쿼리들
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        ORDER BY usage_count DESC, name ASC
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE id = ?
    )";
    
    const std::string FIND_BY_CATEGORY = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE category = ? AND is_enabled = 1
        ORDER BY usage_count DESC, name ASC
    )";
    
    const std::string INSERT = R"(
        INSERT INTO script_library (
            tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE script_library SET
            tenant_id = ?, name = ?, display_name = ?, description = ?,
            category = ?, script_code = ?, parameters = ?, return_type = ?,
            tags = ?, example_usage = ?, is_system = ?, is_template = ?,
            usage_count = ?, rating = ?, version = ?, author = ?,
            license = ?, dependencies = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string UPDATE_BY_ID = UPDATE;
    
    const std::string DELETE_BY_ID = "DELETE FROM script_library WHERE id = ?";
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM script_library WHERE id = ?";
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM script_library";
    const std::string INCREMENT_USAGE_COUNT = "UPDATE script_library SET usage_count = usage_count + 1 WHERE id = ?";
    
    // 특화 조회 쿼리들
    const std::string FIND_BY_IDS = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE id IN (%IN_CLAUSE%)
        ORDER BY usage_count DESC, name ASC
    )";
    
    const std::string DELETE_BY_IDS = "DELETE FROM script_library WHERE id IN (%IN_CLAUSE%)";
    
    const std::string FIND_BY_TENANT_ID = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE tenant_id = ?
        ORDER BY usage_count DESC, name ASC
    )";
    
    const std::string FIND_SYSTEM_SCRIPTS = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE is_system = 1 AND is_enabled = 1
        ORDER BY usage_count DESC, name ASC
    )";
    
    const std::string FIND_TOP_USED = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE is_enabled = 1
        ORDER BY usage_count DESC, name ASC
        LIMIT ?
    )";
    
    const std::string FIND_BY_NAME = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE tenant_id = ? AND name = ?
        ORDER BY version DESC
        LIMIT 1
    )";
    
    // 템플릿 관련 쿼리들
    const std::string FIND_TEMPLATES = R"(
        SELECT 
            id, category, name, description, template_code, variables,
            industry, equipment_type, created_at
        FROM script_templates 
        ORDER BY category, name
    )";
    
    const std::string FIND_TEMPLATES_BY_CATEGORY = R"(
        SELECT 
            id, category, name, description, template_code, variables,
            industry, equipment_type, created_at
        FROM script_templates 
        WHERE category = ?
        ORDER BY name
    )";
    
    const std::string FIND_TEMPLATE_BY_ID = R"(
        SELECT 
            id, category, name, description, template_code, variables,
            industry, equipment_type, created_at
        FROM script_templates 
        WHERE id = ?
    )";
    
    // 통계 관련 쿼리들
    const std::string COUNT_BY_TENANT = "SELECT COUNT(*) as count FROM script_library WHERE tenant_id = ?";
    const std::string COUNT_ALL_SCRIPTS = "SELECT COUNT(*) as count FROM script_library";
    
    const std::string GROUP_BY_CATEGORY_AND_TENANT = R"(
        SELECT category, COUNT(*) as count 
        FROM script_library 
        WHERE tenant_id = ?
        GROUP BY category 
        ORDER BY count DESC
    )";
    
    const std::string GROUP_BY_CATEGORY = R"(
        SELECT category, COUNT(*) as count 
        FROM script_library 
        GROUP BY category 
        ORDER BY count DESC
    )";
    
    const std::string USAGE_STATISTICS_BY_TENANT = R"(
        SELECT 
            AVG(usage_count) as avg_usage,
            MAX(usage_count) as max_usage,
            MIN(usage_count) as min_usage,
            SUM(usage_count) as total_usage,
            COUNT(*) as script_count
        FROM script_library 
        WHERE tenant_id = ?
    )";
    
    const std::string USAGE_STATISTICS = R"(
        SELECT 
            AVG(usage_count) as avg_usage,
            MAX(usage_count) as max_usage,
            MIN(usage_count) as min_usage,
            SUM(usage_count) as total_usage,
            COUNT(*) as script_count
        FROM script_library
    )";
    
} // namespace ScriptLibrary

// =============================================================================
// VirtualPoint 관련 쿼리들 (FOREIGN KEY 제거됨)
// =============================================================================
namespace VirtualPoint {
    
    // 테이블 생성 (SQLite 호환 - FOREIGN KEY 없음)
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_points (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL DEFAULT 0,
            site_id INTEGER,
            device_id INTEGER,
            name VARCHAR(100) NOT NULL,
            description TEXT,
            formula TEXT NOT NULL,
            data_type VARCHAR(20) NOT NULL DEFAULT 'REAL',
            unit VARCHAR(20),
            calculation_interval INTEGER DEFAULT 1000,
            calculation_trigger VARCHAR(20) DEFAULT 'timer',
            execution_type VARCHAR(20) DEFAULT 'JAVASCRIPT',
            cache_duration_ms INTEGER DEFAULT 5000,
            is_enabled INTEGER DEFAULT 1,
            category VARCHAR(50),
            tags TEXT DEFAULT '',
            scope_type VARCHAR(20) NOT NULL DEFAULT 'tenant',
            execution_count INTEGER DEFAULT 0,
            last_value REAL DEFAULT 0.0,
            last_error TEXT DEFAULT '',
            avg_execution_time_ms REAL DEFAULT 0.0,
            created_by VARCHAR(100) DEFAULT 'system',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    // 기본 CRUD 쿼리들
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, execution_type, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        ORDER BY tenant_id, name
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, execution_type, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE id = ?
    )";
    
    const std::string INSERT = R"(
        INSERT INTO virtual_points (
            tenant_id, site_id, device_id, name, description, formula,
            data_type, unit, calculation_interval, calculation_trigger, execution_type, cache_duration_ms,
            is_enabled, category, tags, scope_type, created_by
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE virtual_points SET 
            tenant_id = ?, site_id = ?, device_id = ?, name = ?, 
            description = ?, formula = ?, data_type = ?, unit = ?,
            calculation_interval = ?, calculation_trigger = ?, execution_type = ?, cache_duration_ms = ?,
            is_enabled = ?, category = ?, tags = ?, scope_type = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string UPDATE_BY_ID = UPDATE;
    
    const std::string DELETE_BY_ID = "DELETE FROM virtual_points WHERE id = ?";
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM virtual_points WHERE id = ?";
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM virtual_points";
    
    // 특화 조회 쿼리들
    const std::string FIND_BY_TENANT = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, execution_type, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE tenant_id = ?
        ORDER BY name
    )";
    
    const std::string FIND_BY_SITE = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, execution_type, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE site_id = ?
        ORDER BY name
    )";
    
    const std::string FIND_BY_DEVICE = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, execution_type, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE device_id = ?
        ORDER BY name
    )";
    
    const std::string FIND_ENABLED = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, execution_type, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE is_enabled = 1
        ORDER BY calculation_interval, name
    )";
    
    const std::string FIND_BY_CATEGORY = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, execution_type, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE category = ?
        ORDER BY name
    )";
    
    const std::string FIND_BY_EXECUTION_TYPE = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, execution_type, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE calculation_trigger = ?
        ORDER BY calculation_interval, name
    )";
    
    const std::string FIND_BY_IDS = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, execution_type, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE id IN (%IN_CLAUSE%)
        ORDER BY name
    )";
    
    const std::string DELETE_BY_IDS = "DELETE FROM virtual_points WHERE id IN (%IN_CLAUSE%)";
    
    // 실행 통계 업데이트 쿼리들
    const std::string UPDATE_EXECUTION_STATS = R"(
        UPDATE virtual_points SET 
            execution_count = execution_count + 1,
            last_value = ?,
            avg_execution_time_ms = ((avg_execution_time_ms * (execution_count - 1)) + ?) / execution_count,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string UPDATE_ERROR_INFO = R"(
        UPDATE virtual_points SET 
            last_error = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string UPDATE_LAST_VALUE = R"(
        UPDATE virtual_points SET 
            last_value = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // 가상포인트 입력 매핑 테이블 (FOREIGN KEY 없음)
    const std::string CREATE_INPUTS_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_point_inputs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            virtual_point_id INTEGER NOT NULL,
            variable_name VARCHAR(50) NOT NULL,
            source_type VARCHAR(20) NOT NULL,
            source_id INTEGER,
            is_required INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    const std::string FIND_INPUTS_BY_VP_ID = R"(
        SELECT 
            id, variable_name, source_type, source_id, is_required
        FROM virtual_point_inputs 
        WHERE virtual_point_id = ?
        ORDER BY variable_name
    )";
    
    const std::string INSERT_INPUT = R"(
        INSERT INTO virtual_point_inputs (
            virtual_point_id, variable_name, source_type, source_id, is_required
        ) VALUES (?, ?, ?, ?, ?)
    )";
    
    const std::string DELETE_INPUTS_BY_VP_ID = R"(
        DELETE FROM virtual_point_inputs WHERE virtual_point_id = ?
    )";
    
} // namespace VirtualPoint

} // namespace SQL
} // namespace Database
} // namespace PulseOne

#endif // EXTENDED_SQL_QUERIES_H