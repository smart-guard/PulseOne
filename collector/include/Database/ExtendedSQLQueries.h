// =============================================================================
// collector/include/Database/ExtendedSQLQueries.h
// 🎯 확장 SQL 쿼리 상수 관리 (SQLQueries.h에서 분리된 추가 기능들)
// Alarm, VirtualPoint, ScriptLibrary 관련 모든 쿼리 포함
// =============================================================================

#ifndef EXTENDED_SQL_QUERIES_H
#define EXTENDED_SQL_QUERIES_H

#include <string>

namespace PulseOne {
namespace Database {
namespace SQL {

// =============================================================================
// 🚨 Alarm 관련 쿼리들 (AlarmRule, AlarmOccurrence)
// =============================================================================
namespace Alarm {

// =============================================================================
// AlarmRule 테이블 쿼리들
// =============================================================================
namespace Rule {
    
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS alarm_rules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER DEFAULT 0,
            name VARCHAR(255) NOT NULL,
            description TEXT,
            target_type VARCHAR(50) NOT NULL,
            target_id INTEGER,
            target_address INTEGER,
            target_tag VARCHAR(255),
            condition_type VARCHAR(20) NOT NULL,
            threshold_value REAL,
            threshold_value2 REAL,
            condition_script TEXT,
            severity VARCHAR(20) DEFAULT 'MEDIUM',
            priority INTEGER DEFAULT 5,
            is_enabled BOOLEAN DEFAULT 1,
            auto_clear BOOLEAN DEFAULT 1,
            clear_delay_seconds INTEGER DEFAULT 0,
            notification_enabled BOOLEAN DEFAULT 1,
            notification_channels TEXT DEFAULT '[]',
            escalation_rules TEXT DEFAULT '{}',
            suppression_rules TEXT DEFAULT '{}',
            context_data TEXT DEFAULT '{}',
            created_by INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            INDEX idx_alarm_rules_tenant (tenant_id),
            INDEX idx_alarm_rules_target (target_type, target_id),
            INDEX idx_alarm_rules_enabled (is_enabled),
            INDEX idx_alarm_rules_severity (severity)
        )
    )";
    
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id,
            target_address, target_tag, condition_type, threshold_value,
            threshold_value2, condition_script, severity, priority,
            is_enabled, auto_clear, clear_delay_seconds, notification_enabled,
            notification_channels, escalation_rules, suppression_rules,
            context_data, created_by, created_at, updated_at
        FROM alarm_rules 
        ORDER BY priority DESC, name
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id,
            target_address, target_tag, condition_type, threshold_value,
            threshold_value2, condition_script, severity, priority,
            is_enabled, auto_clear, clear_delay_seconds, notification_enabled,
            notification_channels, escalation_rules, suppression_rules,
            context_data, created_by, created_at, updated_at
        FROM alarm_rules 
        WHERE id = ?
    )";
    
    const std::string INSERT = R"(
        INSERT INTO alarm_rules (
            tenant_id, name, description, target_type, target_id,
            target_address, target_tag, condition_type, threshold_value,
            threshold_value2, condition_script, severity, priority,
            is_enabled, auto_clear, clear_delay_seconds, notification_enabled,
            notification_channels, escalation_rules, suppression_rules,
            context_data, created_by
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE_BY_ID = R"(
        UPDATE alarm_rules 
        SET tenant_id = ?, name = ?, description = ?, target_type = ?,
            target_id = ?, target_address = ?, target_tag = ?,
            condition_type = ?, threshold_value = ?, threshold_value2 = ?,
            condition_script = ?, severity = ?, priority = ?, is_enabled = ?,
            auto_clear = ?, clear_delay_seconds = ?, notification_enabled = ?,
            notification_channels = ?, escalation_rules = ?, suppression_rules = ?,
            context_data = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM alarm_rules WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM alarm_rules WHERE id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM alarm_rules";
    
    const std::string FIND_BY_TARGET = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id,
            target_address, target_tag, condition_type, threshold_value,
            threshold_value2, condition_script, severity, priority,
            is_enabled, auto_clear, clear_delay_seconds, notification_enabled,
            notification_channels, escalation_rules, suppression_rules,
            context_data, created_by, created_at, updated_at
        FROM alarm_rules 
        WHERE target_type = ? AND target_id = ? AND is_enabled = 1
        ORDER BY priority DESC
    )";
    
    const std::string FIND_ENABLED = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id,
            target_address, target_tag, condition_type, threshold_value,
            threshold_value2, condition_script, severity, priority,
            is_enabled, auto_clear, clear_delay_seconds, notification_enabled,
            notification_channels, escalation_rules, suppression_rules,
            context_data, created_by, created_at, updated_at
        FROM alarm_rules 
        WHERE is_enabled = 1
        ORDER BY priority DESC, name
    )";

} // namespace Rule

// =============================================================================
// AlarmOccurrence 테이블 쿼리들
// =============================================================================
namespace Occurrence {
    
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS alarm_occurrences (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_id INTEGER NOT NULL,
            tenant_id INTEGER DEFAULT 0,
            occurrence_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            trigger_value TEXT,
            trigger_condition TEXT,
            alarm_message TEXT NOT NULL,
            severity VARCHAR(20) DEFAULT 'MEDIUM',
            state VARCHAR(20) DEFAULT 'active',
            acknowledged_time DATETIME,
            acknowledged_by INTEGER,
            acknowledge_comment TEXT,
            cleared_time DATETIME,
            cleared_value TEXT,
            clear_comment TEXT,
            notification_sent BOOLEAN DEFAULT 0,
            notification_time DATETIME,
            notification_count INTEGER DEFAULT 0,
            notification_result TEXT,
            context_data TEXT,
            source_name VARCHAR(255),
            location VARCHAR(255),
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (rule_id) REFERENCES alarm_rules(id) ON DELETE CASCADE,
            
            INDEX idx_alarm_occurrences_state (state),
            INDEX idx_alarm_occurrences_tenant (tenant_id),
            INDEX idx_alarm_occurrences_rule (rule_id),
            INDEX idx_alarm_occurrences_severity (severity),
            INDEX idx_alarm_occurrences_occurrence_time (occurrence_time)
        )
    )";
    
    const std::string FIND_ALL = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value,
            trigger_condition, alarm_message, severity, state,
            acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent,
            notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value,
            trigger_condition, alarm_message, severity, state,
            acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent,
            notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE id = ?
    )";
    
    const std::string FIND_ACTIVE_BY_RULE_ID = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value,
            trigger_condition, alarm_message, severity, state,
            acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent,
            notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE rule_id = ? AND state IN ('active', 'acknowledged')
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_ACTIVE = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value,
            trigger_condition, alarm_message, severity, state,
            acknowledged_time, acknowledged_by, acknowledge_comment,
            cleared_time, cleared_value, clear_comment, notification_sent,
            notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE state = 'active'
        ORDER BY severity DESC, occurrence_time DESC
    )";

} // namespace Occurrence

} // namespace Alarm

// =============================================================================
// 🧮 VirtualPoint 관련 쿼리들
// =============================================================================
namespace VirtualPoint {
    
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_points (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER DEFAULT 0,
            name VARCHAR(255) NOT NULL,
            display_name VARCHAR(255),
            description TEXT,
            unit VARCHAR(50),
            data_type VARCHAR(20) DEFAULT 'FLOAT',
            formula TEXT NOT NULL,
            execution_type VARCHAR(20) DEFAULT 'JAVASCRIPT',
            execution_interval_ms INTEGER DEFAULT 5000,
            is_enabled BOOLEAN DEFAULT 1,
            error_handling VARCHAR(20) DEFAULT 'RETURN_NULL',
            default_value REAL DEFAULT 0.0,
            min_value REAL,
            max_value REAL,
            decimal_places INTEGER DEFAULT 2,
            tags TEXT DEFAULT '[]',
            context_data TEXT DEFAULT '{}',
            last_calculated_value REAL,
            last_calculation_time DATETIME,
            last_error_message TEXT,
            calculation_count INTEGER DEFAULT 0,
            error_count INTEGER DEFAULT 0,
            created_by INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            UNIQUE(tenant_id, name),
            
            INDEX idx_virtual_points_tenant (tenant_id),
            INDEX idx_virtual_points_enabled (is_enabled),
            INDEX idx_virtual_points_execution_type (execution_type),
            INDEX idx_virtual_points_name (name)
        )
    )";
    
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, name, display_name, description, unit,
            data_type, formula, execution_type, execution_interval_ms,
            is_enabled, error_handling, default_value, min_value, max_value,
            decimal_places, tags, context_data, last_calculated_value,
            last_calculation_time, last_error_message, calculation_count,
            error_count, created_by, created_at, updated_at
        FROM virtual_points 
        ORDER BY name
    )";
    
    const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, name, display_name, description, unit,
            data_type, formula, execution_type, execution_interval_ms,
            is_enabled, error_handling, default_value, min_value, max_value,
            decimal_places, tags, context_data, last_calculated_value,
            last_calculation_time, last_error_message, calculation_count,
            error_count, created_by, created_at, updated_at
        FROM virtual_points 
        WHERE id = ?
    )";
    
    const std::string FIND_ENABLED = R"(
        SELECT 
            id, tenant_id, name, display_name, description, unit,
            data_type, formula, execution_type, execution_interval_ms,
            is_enabled, error_handling, default_value, min_value, max_value,
            decimal_places, tags, context_data, last_calculated_value,
            last_calculation_time, last_error_message, calculation_count,
            error_count, created_by, created_at, updated_at
        FROM virtual_points 
        WHERE is_enabled = 1
        ORDER BY name
    )";

} // namespace VirtualPoint

// =============================================================================
// 📚 ScriptLibrary 관련 쿼리들
// =============================================================================
namespace ScriptLibrary {
    
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
            return_type VARCHAR(20) DEFAULT 'FLOAT',
            tags TEXT DEFAULT '[]',
            example_usage TEXT,
            is_system BOOLEAN DEFAULT 0,
            is_template BOOLEAN DEFAULT 0,
            usage_count INTEGER DEFAULT 0,
            rating REAL DEFAULT 0.0,
            version VARCHAR(20) DEFAULT '1.0.0',
            author VARCHAR(100),
            license VARCHAR(50),
            dependencies TEXT DEFAULT '[]',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            UNIQUE(tenant_id, name),
            
            INDEX idx_script_library_tenant (tenant_id),
            INDEX idx_script_library_category (category),
            INDEX idx_script_library_name (name),
            INDEX idx_script_library_usage (usage_count),
            INDEX idx_script_library_system (is_system)
        )
    )";
    
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        ORDER BY name
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
    
    const std::string INSERT = R"(
        INSERT INTO script_library (
            tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE_BY_ID = R"(
        UPDATE script_library 
        SET tenant_id = ?, name = ?, display_name = ?, description = ?, 
            category = ?, script_code = ?, parameters = ?, return_type = ?, 
            tags = ?, example_usage = ?, is_system = ?, is_template = ?, 
            usage_count = ?, rating = ?, version = ?, author = ?, 
            license = ?, dependencies = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM script_library WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM script_library WHERE id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM script_library";
    
    const std::string FIND_BY_CATEGORY = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE category = ?
        ORDER BY name
    )";
    
    const std::string FIND_BY_TENANT_ID = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE tenant_id = ? OR is_system = 1 
        ORDER BY name
    )";
    
    const std::string FIND_SYSTEM_SCRIPTS = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE is_system = 1
        ORDER BY category, name
    )";
    
    const std::string FIND_TOP_USED = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE usage_count > 0
        ORDER BY usage_count DESC, rating DESC
        LIMIT ?
    )";
    
    const std::string INCREMENT_USAGE_COUNT = R"(
        UPDATE script_library 
        SET usage_count = usage_count + 1,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

} // namespace ScriptLibrary

} // namespace SQL
} // namespace Database  
} // namespace PulseOne

#endif // EXTENDED_SQL_QUERIES_H