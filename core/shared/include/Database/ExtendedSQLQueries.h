// =============================================================================
// collector/include/Database/ExtendedSQLQueries.h
// 🔧 완전 SQLite 호환 버전 - 실제 스키마에 맞춰 수정
// =============================================================================

#ifndef EXTENDED_SQL_QUERIES_H
#define EXTENDED_SQL_QUERIES_H

#include <string>

namespace PulseOne {
namespace Database {
namespace SQL {

// =============================================================================
// AlarmOccurrence 관련 쿼리들 (실제 스키마에 맞춤)
// =============================================================================
namespace AlarmOccurrence {

// 테이블 생성 - cleared_by 필드 포함
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
            cleared_by INTEGER,
            notification_sent INTEGER DEFAULT 0,
            notification_time DATETIME,
            notification_count INTEGER DEFAULT 0,
            notification_result TEXT,
            context_data TEXT,
            source_name VARCHAR(100),
            location VARCHAR(200),
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            device_id INTEGER,
            point_id INTEGER,
            category VARCHAR(50) DEFAULT NULL,
            tags TEXT DEFAULT NULL
        )
    )";

// 기본 CRUD 쿼리들 - cleared_by 포함
const std::string FIND_ALL = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at,
            device_id, point_id, category, tags
        FROM alarm_occurrences 
        ORDER BY occurrence_time DESC
    )";

const std::string FIND_BY_ID = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at,
            device_id, point_id, category, tags
        FROM alarm_occurrences 
        WHERE id = ?
    )";

// ⭐ 중복 제거: FIND_ACTIVE는 하나만 유지
const std::string FIND_ACTIVE = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at,
            device_id, point_id, category, tags
        FROM alarm_occurrences 
        WHERE UPPER(state) IN ('ACTIVE', 'ACKNOWLEDGED')
        ORDER BY occurrence_time DESC
    )";

// ⭐ 중복 제거: FIND_BY_RULE_ID는 하나만 유지
const std::string FIND_BY_RULE_ID = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at,
            device_id, point_id, category, tags
        FROM alarm_occurrences 
        WHERE rule_id = ?
        ORDER BY occurrence_time DESC
    )";

// ⭐ 누락된 INSERT 쿼리 추가
const std::string INSERT = R"(
        INSERT INTO alarm_occurrences (
            rule_id, tenant_id, trigger_value, trigger_condition,
            alarm_message, severity, context_data, source_name, location,
            device_id, point_id, category, tags
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string INSERT_NAMED = R"(
        INSERT INTO alarm_occurrences (
            rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, context_data, source_name, location,
            device_id, point_id, category, tags, cleared_by,
            created_at, updated_at
        ) VALUES (
            {rule_id}, {tenant_id}, {occurrence_time}, {trigger_value}, {trigger_condition},
            {alarm_message}, {severity}, {state}, {context_data}, {source_name}, {location},
            {device_id}, {point_id}, {category}, {tags}, {cleared_by},
            datetime('now','localtime'), datetime('now','localtime')
        )
    )";

// ⭐ 누락된 UPDATE 쿼리 추가 (필드 확장 및 named placeholder 적용)
const std::string UPDATE = R"(
        UPDATE alarm_occurrences SET
            occurrence_time = {occurrence_time},
            trigger_value = {trigger_value}, 
            trigger_condition = {trigger_condition}, 
            alarm_message = {alarm_message},
            severity = {severity}, 
            state = {state}, 
            acknowledged_time = {acknowledged_time},
            cleared_time = {cleared_time},
            cleared_by = {cleared_by},
            context_data = {context_data}, 
            source_name = {source_name},
            location = {location}, 
            device_id = {device_id}, 
            point_id = {point_id}, 
            category = {category}, 
            tags = {tags},
            updated_at = datetime('now','localtime')
        WHERE id = {id}
    )";

// 상태 관리 쿼리들
const std::string ACKNOWLEDGE = R"(
        UPDATE alarm_occurrences SET
            state = 'acknowledged',
            acknowledged_time = datetime('now','localtime'),
            acknowledged_by = ?,
            acknowledge_comment = ?,
            updated_at = datetime('now','localtime')
        WHERE id = ?
    )";

const std::string CLEAR = R"(
        UPDATE alarm_occurrences SET
            state = 'cleared',
            cleared_time = datetime('now','localtime'),
            cleared_value = ?,
            clear_comment = ?,
            cleared_by = ?,
            updated_at = datetime('now','localtime')
        WHERE id = ?
    )";

const std::string CLEAR_ALARM = CLEAR;

// 기타 유틸리티 쿼리들
const std::string DELETE_BY_ID = "DELETE FROM alarm_occurrences WHERE id = ?";
const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM alarm_occurrences WHERE id = ?";
const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM alarm_occurrences";
const std::string FIND_MAX_ID =
    "SELECT COALESCE(MAX(id), 0) as max_id FROM alarm_occurrences";

// 감사 추적용 쿼리들
const std::string FIND_CLEARED_BY_USER = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at,
            device_id, point_id, category, tags
        FROM alarm_occurrences 
        WHERE cleared_by = ?
        ORDER BY cleared_time DESC
    )";

const std::string FIND_ACKNOWLEDGED_BY_USER = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at,
            device_id, point_id, category, tags
        FROM alarm_occurrences 
        WHERE acknowledged_by = ?
        ORDER BY acknowledged_time DESC
    )";

} // namespace AlarmOccurrence

// =============================================================================
// AlarmRule 관련 쿼리들 (실제 스키마에 맞춤)
// =============================================================================
namespace AlarmRule {

// 테이블 생성 - 실제 스키마와 동일
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
            template_id INTEGER,
            rule_group VARCHAR(36),
            created_by_template INTEGER DEFAULT 0,
            last_template_update DATETIME,
            escalation_enabled INTEGER DEFAULT 0,
            escalation_max_level INTEGER DEFAULT 3,
            escalation_rules TEXT DEFAULT NULL,
            category VARCHAR(50) DEFAULT NULL,
            tags TEXT DEFAULT NULL,
            FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
            FOREIGN KEY (created_by) REFERENCES users(id)
        )
    )";

// 기본 CRUD 쿼리들 - 모든 컬럼 포함
const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_at, updated_at, created_by, template_id, rule_group,
            created_by_template, last_template_update, escalation_enabled,
            escalation_max_level, escalation_rules, category, tags
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
            created_at, updated_at, created_by, template_id, rule_group,
            created_by_template, last_template_update, escalation_enabled,
            escalation_max_level, escalation_rules, category, tags
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
            created_at, updated_at, created_by, template_id, rule_group,
            created_by_template, last_template_update, escalation_enabled,
            escalation_max_level, escalation_rules, category, tags
        FROM alarm_rules 
        WHERE target_type = ? AND target_id = ? AND is_enabled = 1 AND is_deleted = 0
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
            created_at, updated_at, created_by, template_id, rule_group,
            created_by_template, last_template_update, escalation_enabled,
            escalation_max_level, escalation_rules, category, tags
        FROM alarm_rules 
        WHERE is_enabled = 1 AND is_deleted = 0
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
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_by, template_id, rule_group, created_by_template,
            escalation_enabled, escalation_max_level, escalation_rules, category, tags
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string UPDATE = R"(
        UPDATE alarm_rules SET
            tenant_id = {tenant_id}, name = {name}, description = {description}, 
            target_type = {target_type}, target_id = {target_id},
            target_group = {target_group}, alarm_type = {alarm_type}, 
            high_high_limit = {high_high_limit}, high_limit = {high_limit},
            low_limit = {low_limit}, low_low_limit = {low_low_limit}, 
            deadband = {deadband}, rate_of_change = {rate_of_change},
            trigger_condition = {trigger_condition}, condition_script = {condition_script}, 
            message_script = {message_script}, message_config = {message_config}, 
            message_template = {message_template}, severity = {severity}, priority = {priority},
            auto_acknowledge = {auto_acknowledge}, acknowledge_timeout_min = {acknowledge_timeout_min}, 
            auto_clear = {auto_clear}, suppression_rules = {suppression_rules}, 
            notification_enabled = {notification_enabled}, notification_delay_sec = {notification_delay_sec},
            notification_repeat_interval_min = {notification_repeat_interval_min}, 
            notification_channels = {notification_channels},
            notification_recipients = {notification_recipients}, 
            is_enabled = {is_enabled}, is_latched = {is_latched},
            template_id = {template_id}, rule_group = {rule_group}, 
            created_by = {created_by},
            updated_at = CURRENT_TIMESTAMP
        WHERE id = {id}
    )";

const std::string DELETE_BY_ID = "DELETE FROM alarm_rules WHERE id = ?";
const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM alarm_rules WHERE id = ?";
const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM alarm_rules";
const std::string COUNT_ENABLED = "SELECT COUNT(*) as count FROM alarm_rules "
                                  "WHERE is_enabled = 1 AND is_deleted = 0";

} // namespace AlarmRule

// =============================================================================
// ScriptLibrary 관련 쿼리들 (실제 스키마에 맞춤)
// =============================================================================
namespace ScriptLibrary {

// 테이블 생성 - 실제 스키마와 동일
const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS script_library (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL DEFAULT 0,
            category VARCHAR(50) NOT NULL,
            name VARCHAR(100) NOT NULL,
            display_name VARCHAR(100),
            description TEXT,
            script_code TEXT NOT NULL,
            parameters TEXT,
            return_type VARCHAR(20) DEFAULT 'number',
            tags TEXT,
            example_usage TEXT,
            is_system INTEGER DEFAULT 0,
            usage_count INTEGER DEFAULT 0,
            rating REAL DEFAULT 0.0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(tenant_id, name)
        )
    )";

// 기본 CRUD 쿼리들 - 실제 컬럼에 맞춤
const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, category, name, display_name, description,
            script_code, parameters, return_type, tags, example_usage,
            is_system, usage_count, rating, created_at, updated_at
        FROM script_library 
        ORDER BY usage_count DESC, name ASC
    )";

const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, category, name, display_name, description,
            script_code, parameters, return_type, tags, example_usage,
            is_system, usage_count, rating, created_at, updated_at
        FROM script_library 
        WHERE id = ?
    )";

const std::string FIND_BY_CATEGORY = R"(
        SELECT 
            id, tenant_id, category, name, display_name, description,
            script_code, parameters, return_type, tags, example_usage,
            is_system, usage_count, rating, created_at, updated_at
        FROM script_library 
        WHERE category = ?
        ORDER BY usage_count DESC, name ASC
    )";

const std::string INSERT = R"(
        INSERT INTO script_library (
            tenant_id, category, name, display_name, description,
            script_code, parameters, return_type, tags, example_usage,
            is_system, usage_count, rating
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string UPDATE = R"(
        UPDATE script_library SET
            tenant_id = ?, category = ?, name = ?, display_name = ?, description = ?,
            script_code = ?, parameters = ?, return_type = ?,
            tags = ?, example_usage = ?, is_system = ?,
            usage_count = ?, rating = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

const std::string UPDATE_BY_ID = UPDATE;

const std::string DELETE_BY_ID = "DELETE FROM script_library WHERE id = ?";
const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM script_library WHERE id = ?";
const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM script_library";
const std::string INCREMENT_USAGE_COUNT =
    "UPDATE script_library SET usage_count = usage_count + 1 WHERE id = ?";

// 특화 조회 쿼리들
const std::string FIND_BY_IDS = R"(
        SELECT 
            id, tenant_id, category, name, display_name, description,
            script_code, parameters, return_type, tags, example_usage,
            is_system, usage_count, rating, created_at, updated_at
        FROM script_library 
        WHERE id IN (%IN_CLAUSE%)
        ORDER BY usage_count DESC, name ASC
    )";

const std::string DELETE_BY_IDS =
    "DELETE FROM script_library WHERE id IN (%IN_CLAUSE%)";

const std::string FIND_BY_TENANT_ID = R"(
        SELECT 
            id, tenant_id, category, name, display_name, description,
            script_code, parameters, return_type, tags, example_usage,
            is_system, usage_count, rating, created_at, updated_at
        FROM script_library 
        WHERE tenant_id = ?
        ORDER BY usage_count DESC, name ASC
    )";

const std::string FIND_SYSTEM_SCRIPTS = R"(
        SELECT 
            id, tenant_id, category, name, display_name, description,
            script_code, parameters, return_type, tags, example_usage,
            is_system, usage_count, rating, created_at, updated_at
        FROM script_library 
        WHERE is_system = 1
        ORDER BY usage_count DESC, name ASC
    )";

const std::string FIND_TOP_USED = R"(
        SELECT 
            id, tenant_id, category, name, display_name, description,
            script_code, parameters, return_type, tags, example_usage,
            is_system, usage_count, rating, created_at, updated_at
        FROM script_library 
        ORDER BY usage_count DESC, name ASC
        LIMIT ?
    )";

const std::string FIND_BY_NAME = R"(
        SELECT 
            id, tenant_id, category, name, display_name, description,
            script_code, parameters, return_type, tags, example_usage,
            is_system, usage_count, rating, created_at, updated_at
        FROM script_library 
        WHERE tenant_id = ? AND name = ?
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
const std::string COUNT_BY_TENANT =
    "SELECT COUNT(*) as count FROM script_library WHERE tenant_id = ?";
const std::string COUNT_ALL_SCRIPTS =
    "SELECT COUNT(*) as count FROM script_library";

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
// VirtualPoint 관련 쿼리들 (실제 스키마에 맞춤)
// =============================================================================
namespace VirtualPoint {

// 테이블 생성 - 실제 스키마와 동일
const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_points (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            scope_type VARCHAR(20) NOT NULL DEFAULT 'tenant',
            site_id INTEGER,
            device_id INTEGER,
            name VARCHAR(100) NOT NULL,
            description TEXT,
            formula TEXT NOT NULL,
            data_type VARCHAR(20) NOT NULL DEFAULT 'float',
            unit VARCHAR(20),
            calculation_interval INTEGER DEFAULT 1000,
            calculation_trigger VARCHAR(20) DEFAULT 'timer',
            is_enabled INTEGER DEFAULT 1,
            category VARCHAR(50),
            tags TEXT,
            execution_type VARCHAR(20) DEFAULT 'javascript',
            dependencies TEXT,
            cache_duration_ms INTEGER DEFAULT 0,
            error_handling VARCHAR(20) DEFAULT 'return_null',
            last_error TEXT,
            execution_count INTEGER DEFAULT 0,
            avg_execution_time_ms REAL DEFAULT 0.0,
            last_execution_time DATETIME,
            created_by INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
            FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
            CONSTRAINT chk_scope_consistency CHECK (
                (scope_type = 'tenant' AND site_id IS NULL AND device_id IS NULL) OR
                (scope_type = 'site' AND site_id IS NOT NULL AND device_id IS NULL) OR
                (scope_type = 'device' AND site_id IS NOT NULL AND device_id IS NOT NULL)
            ),
            CONSTRAINT chk_scope_type CHECK (scope_type IN ('tenant', 'site', 'device'))
        )
    )";

// 기본 CRUD 쿼리들 - 실제 컬럼에 맞춤
const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, scope_type, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, last_error,
            execution_count, avg_execution_time_ms, last_execution_time,
            created_by, created_at, updated_at
        FROM virtual_points 
        ORDER BY tenant_id, name
    )";

const std::string FIND_BY_ID = R"(
        SELECT 
            id, tenant_id, scope_type, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, last_error,
            execution_count, avg_execution_time_ms, last_execution_time,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE id = ?
    )";

const std::string INSERT = R"(
        INSERT INTO virtual_points (
            tenant_id, scope_type, site_id, device_id, name, description, formula,
            data_type, unit, calculation_interval, calculation_trigger,
            is_enabled, category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, created_by
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string UPDATE = R"(
        UPDATE virtual_points SET 
            tenant_id = ?, scope_type = ?, site_id = ?, device_id = ?,
            name = ?, description = ?, formula = ?, data_type = ?, unit = ?,
            calculation_interval = ?, calculation_trigger = ?,
            is_enabled = ?, category = ?, tags = ?, execution_type = ?,
            dependencies = ?, cache_duration_ms = ?, error_handling = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

const std::string UPDATE_BY_ID = UPDATE;

const std::string DELETE_BY_ID = "DELETE FROM virtual_points WHERE id = ?";
const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM virtual_points WHERE id = ?";
const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM virtual_points";

// 특화 조회 쿼리들
const std::string FIND_BY_TENANT = R"(
        SELECT
            id, tenant_id, scope_type, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, last_error,
            execution_count, avg_execution_time_ms, last_execution_time,
            created_by, created_at, updated_at
        FROM virtual_points
        WHERE tenant_id = ?
        ORDER BY name
    )";

const std::string FIND_BY_SITE = R"(
        SELECT 
            id, tenant_id, scope_type, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, last_error,
            execution_count, avg_execution_time_ms, last_execution_time,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE site_id = ?
        ORDER BY name
    )";

const std::string FIND_BY_DEVICE = R"(
        SELECT 
            id, tenant_id, scope_type, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, last_error,
            execution_count, avg_execution_time_ms, last_execution_time,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE device_id = ?
        ORDER BY name
    )";

const std::string FIND_ENABLED = R"(
        SELECT 
            id, tenant_id, scope_type, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, last_error,
            execution_count, avg_execution_time_ms, last_execution_time,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE is_enabled = 1
        ORDER BY calculation_interval, name
    )";

const std::string FIND_BY_CATEGORY = R"(
        SELECT 
            id, tenant_id, scope_type, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, last_error,
            execution_count, avg_execution_time_ms, last_execution_time,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE category = ?
        ORDER BY name
    )";

const std::string FIND_BY_EXECUTION_TYPE = R"(
        SELECT 
            id, tenant_id, scope_type, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, last_error,
            execution_count, avg_execution_time_ms, last_execution_time,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE calculation_trigger = ?
        ORDER BY calculation_interval, name
    )";

const std::string FIND_BY_IDS = R"(
        SELECT 
            id, tenant_id, scope_type, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, is_enabled,
            category, tags, execution_type, dependencies,
            cache_duration_ms, error_handling, last_error,
            execution_count, avg_execution_time_ms, last_execution_time,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE id IN (%IN_CLAUSE%)
        ORDER BY name
    )";

const std::string DELETE_BY_IDS =
    "DELETE FROM virtual_points WHERE id IN (%IN_CLAUSE%)";

// 실행 통계 업데이트 쿼리들
const std::string UPDATE_EXECUTION_STATS = R"(
        UPDATE virtual_points SET 
            execution_count = execution_count + 1,
            avg_execution_time_ms = ((avg_execution_time_ms * (execution_count - 1)) + ?) / execution_count,
            last_execution_time = CURRENT_TIMESTAMP,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

const std::string UPDATE_ERROR_INFO = R"(
        UPDATE virtual_points SET 
            last_error = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

const std::string UPDATE_EXECUTION_TIME = R"(
        UPDATE virtual_points SET 
            last_execution_time = CURRENT_TIMESTAMP,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

// 가상포인트 입력 매핑 테이블 - 실제 스키마와 동일
const std::string CREATE_INPUTS_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_point_inputs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            virtual_point_id INTEGER NOT NULL,
            variable_name VARCHAR(50) NOT NULL,
            source_type VARCHAR(20) NOT NULL,
            source_id INTEGER,
            constant_value REAL,
            source_formula TEXT,
            data_processing VARCHAR(20) DEFAULT 'current',
            time_window_seconds INTEGER,
            FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
            UNIQUE(virtual_point_id, variable_name),
            CONSTRAINT chk_source_type CHECK (source_type IN ('data_point', 'virtual_point', 'constant', 'formula'))
        )
    )";

const std::string FIND_INPUTS_BY_VP_ID = R"(
        SELECT 
            id, virtual_point_id, variable_name, source_type, source_id,
            constant_value, source_formula, data_processing, time_window_seconds
        FROM virtual_point_inputs 
        WHERE virtual_point_id = ?
        ORDER BY variable_name
    )";

const std::string INSERT_INPUT = R"(
        INSERT INTO virtual_point_inputs (
            virtual_point_id, variable_name, source_type, source_id,
            constant_value, source_formula, data_processing, time_window_seconds
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string DELETE_INPUTS_BY_VP_ID = R"(
        DELETE FROM virtual_point_inputs WHERE virtual_point_id = ?
    )";

// 가상포인트 값 관련 쿼리들
const std::string CREATE_VALUES_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_point_values (
            virtual_point_id INTEGER PRIMARY KEY,
            value REAL,
            string_value TEXT,
            quality VARCHAR(20) DEFAULT 'good',
            last_calculated DATETIME DEFAULT CURRENT_TIMESTAMP,
            calculation_error TEXT,
            input_values TEXT,
            FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
        )
    )";

const std::string FIND_VALUE_BY_VP_ID = R"(
        SELECT 
            virtual_point_id, value, string_value, quality,
            last_calculated, calculation_error, input_values
        FROM virtual_point_values 
        WHERE virtual_point_id = ?
    )";

const std::string INSERT_OR_UPDATE_VALUE = R"(
        INSERT OR REPLACE INTO virtual_point_values (
            virtual_point_id, value, string_value, quality,
            last_calculated, calculation_error, input_values
        ) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP, ?, ?)
    )";

const std::string UPDATE_VALUE = R"(
        UPDATE virtual_point_values SET 
            value = ?, string_value = ?, quality = ?,
            last_calculated = CURRENT_TIMESTAMP,
            calculation_error = ?, input_values = ?
        WHERE virtual_point_id = ?
    )";

const std::string DELETE_VALUE_BY_VP_ID = R"(
        DELETE FROM virtual_point_values WHERE virtual_point_id = ?
    )";

} // namespace VirtualPoint

// =============================================================================
// 추가 인덱스 생성 쿼리들
// =============================================================================
namespace Indexes {

// AlarmOccurrence 인덱스들
const std::string CREATE_ALARM_OCC_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_rule_id ON alarm_occurrences(rule_id);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_tenant_id ON alarm_occurrences(tenant_id);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_state ON alarm_occurrences(state);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_severity ON alarm_occurrences(severity);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_occurrence_time ON alarm_occurrences(occurrence_time DESC);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_device_id ON alarm_occurrences(device_id);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_point_id ON alarm_occurrences(point_id);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_category ON alarm_occurrences(category);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_acknowledged_by ON alarm_occurrences(acknowledged_by);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_cleared_by ON alarm_occurrences(cleared_by);
        CREATE INDEX IF NOT EXISTS idx_alarm_occ_cleared_time ON alarm_occurrences(cleared_time DESC);
    )";

// AlarmRule 인덱스들
const std::string CREATE_ALARM_RULE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_alarm_rules_tenant ON alarm_rules(tenant_id);
        CREATE INDEX IF NOT EXISTS idx_alarm_rules_target ON alarm_rules(target_type, target_id);
        CREATE INDEX IF NOT EXISTS idx_alarm_rules_enabled ON alarm_rules(is_enabled);
        CREATE INDEX IF NOT EXISTS idx_alarm_rules_template_id ON alarm_rules(template_id);
        CREATE INDEX IF NOT EXISTS idx_alarm_rules_rule_group ON alarm_rules(rule_group);
        CREATE INDEX IF NOT EXISTS idx_alarm_rules_created_by_template ON alarm_rules(created_by_template);
        CREATE INDEX IF NOT EXISTS idx_alarm_rules_category ON alarm_rules(category);
        CREATE INDEX IF NOT EXISTS idx_alarm_rules_tags ON alarm_rules(tags);
    )";

// ScriptLibrary 인덱스들
const std::string CREATE_SCRIPT_LIB_INDEXES = R"(
        CREATE UNIQUE INDEX IF NOT EXISTS idx_script_library_tenant_name ON script_library(tenant_id, name);
        CREATE INDEX IF NOT EXISTS idx_script_library_category ON script_library(category);
        CREATE INDEX IF NOT EXISTS idx_script_library_tenant ON script_library(tenant_id);
        CREATE INDEX IF NOT EXISTS idx_script_library_is_system ON script_library(is_system);
        CREATE INDEX IF NOT EXISTS idx_script_library_usage_count ON script_library(usage_count DESC);
    )";

// VirtualPoint 인덱스들
const std::string CREATE_VP_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_virtual_points_tenant ON virtual_points(tenant_id);
        CREATE INDEX IF NOT EXISTS idx_virtual_points_scope ON virtual_points(scope_type);
        CREATE INDEX IF NOT EXISTS idx_virtual_points_site ON virtual_points(site_id);
        CREATE INDEX IF NOT EXISTS idx_virtual_points_device ON virtual_points(device_id);
        CREATE INDEX IF NOT EXISTS idx_virtual_points_enabled ON virtual_points(is_enabled);
        CREATE INDEX IF NOT EXISTS idx_virtual_points_category ON virtual_points(category);
        CREATE INDEX IF NOT EXISTS idx_virtual_points_trigger ON virtual_points(calculation_trigger);
    )";

// VirtualPointInputs 인덱스들
const std::string CREATE_VP_INPUTS_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_vp_inputs_virtual_point ON virtual_point_inputs(virtual_point_id);
        CREATE INDEX IF NOT EXISTS idx_vp_inputs_source ON virtual_point_inputs(source_type, source_id);
    )";

// VirtualPointValues 인덱스들
const std::string CREATE_VP_VALUES_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_vp_values_calculated ON virtual_point_values(last_calculated DESC);
    )";
} // namespace Indexes

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
            template_id INTEGER,
            export_mode VARCHAR(20) DEFAULT 'on_change',
            export_interval INTEGER DEFAULT 0,
            batch_size INTEGER DEFAULT 100,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL,
            FOREIGN KEY (template_id) REFERENCES payload_templates(id) ON DELETE SET NULL
        )
    )";

const std::string CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_export_targets_type ON export_targets(target_type);
        CREATE INDEX IF NOT EXISTS idx_export_targets_profile ON export_targets(profile_id);
        CREATE INDEX IF NOT EXISTS idx_export_targets_enabled ON export_targets(is_enabled);
        CREATE INDEX IF NOT EXISTS idx_export_targets_name ON export_targets(name);
        CREATE INDEX IF NOT EXISTS idx_export_targets_template ON export_targets(template_id);
    )";

// 기본 CRUD
const std::string FIND_ALL = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               template_id, export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets
        ORDER BY name ASC
    )";

const std::string FIND_BY_ID = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               template_id, export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE id = ?
    )";

const std::string FIND_BY_NAME = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               template_id, export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE name = ?
    )";

const std::string FIND_BY_ENABLED = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               template_id, export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE is_enabled = ? ORDER BY name ASC
    )";

const std::string FIND_BY_TARGET_TYPE = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               template_id, export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE target_type = ? ORDER BY name ASC
    )";

const std::string FIND_BY_PROFILE_ID = R"(
        SELECT id, profile_id, name, target_type, description, is_enabled, config,
               template_id, export_mode, export_interval, batch_size, created_at, updated_at
        FROM export_targets WHERE profile_id = ? ORDER BY name ASC
    )";

const std::string INSERT = R"(
        INSERT INTO export_targets (
            profile_id, name, target_type, description, is_enabled, config,
            template_id, export_mode, export_interval, batch_size
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string UPDATE = R"(
        UPDATE export_targets SET
            profile_id = ?, name = ?, target_type = ?, description = ?, is_enabled = ?,
            config = ?, template_id = ?, export_mode = ?, export_interval = ?, batch_size = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

const std::string DELETE_BY_ID = "DELETE FROM export_targets WHERE id = ?";

const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM export_targets WHERE id = ?";

const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM export_targets";

const std::string COUNT_ACTIVE =
    "SELECT COUNT(*) as count FROM export_targets WHERE is_enabled = 1";

const std::string COUNT_BY_TYPE = R"(
        SELECT target_type, COUNT(*) as count 
        FROM export_targets 
        GROUP BY target_type
    )";

const std::string FIND_WITH_TEMPLATE = R"(
        SELECT 
            t.id, t.profile_id, t.name, t.target_type, t.description, t.is_enabled, t.config,
            t.template_id, t.export_mode, t.export_interval, t.batch_size, 
            t.created_at, t.updated_at,
            p.template_json, p.system_type as template_system_type, p.name as template_name
        FROM export_targets t
        LEFT JOIN payload_templates p ON t.template_id = p.id
        WHERE t.id = ?
    )";

const std::string FIND_ALL_WITH_TEMPLATE = R"(
        SELECT 
            t.id, t.profile_id, t.name, t.target_type, t.description, t.is_enabled, t.config,
            t.template_id, t.export_mode, t.export_interval, t.batch_size, 
            t.created_at, t.updated_at,
            p.template_json, p.system_type as template_system_type, p.name as template_name
        FROM export_targets t
        LEFT JOIN payload_templates p ON t.template_id = p.id
        WHERE t.is_enabled = 1
        ORDER BY t.name ASC
    )";

} // namespace ExportTarget

// =============================================================================
// 🎯 ExportTargetMapping 쿼리들 (export_target_mappings 테이블)
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

const std::string DELETE_BY_ID =
    "DELETE FROM export_target_mappings WHERE id = ?";

const std::string DELETE_BY_TARGET_ID =
    "DELETE FROM export_target_mappings WHERE target_id = ?";

const std::string DELETE_BY_POINT_ID =
    "DELETE FROM export_target_mappings WHERE point_id = ?";

const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM export_target_mappings WHERE id = ?";

const std::string COUNT_BY_TARGET_ID =
    "SELECT COUNT(*) as count FROM export_target_mappings WHERE target_id = ?";

const std::string COUNT_BY_POINT_ID =
    "SELECT COUNT(*) as count FROM export_target_mappings WHERE point_id = ?";

} // namespace ExportTargetMapping

// =============================================================================
// 🎯 ExportLog 쿼리들 (export_logs 테이블)
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
            gateway_id INTEGER,
            sent_payload TEXT,
            
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
               response_data, http_status_code, processing_time_ms, timestamp, client_info,
               gateway_id, sent_payload
        FROM export_logs
        ORDER BY timestamp DESC LIMIT 1000
    )";

const std::string FIND_BY_ID = R"(
        SELECT id, log_type, service_id, target_id, mapping_id, point_id,
               source_value, converted_value, status, error_message, error_code,
               response_data, http_status_code, processing_time_ms, timestamp, client_info,
               gateway_id, sent_payload
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
            response_data, http_status_code, processing_time_ms, timestamp, client_info,
            gateway_id, sent_payload, tenant_id
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string UPDATE = R"(
        UPDATE export_logs SET
            log_type = ?, service_id = ?, target_id = ?, mapping_id = ?, point_id = ?,
            source_value = ?, converted_value = ?, status = ?, error_message = ?,
            error_code = ?, response_data = ?, http_status_code = ?,
            processing_time_ms = ?, timestamp = ?, client_info = ?,
            gateway_id = ?, sent_payload = ?
        WHERE id = ?
    )";

const std::string DELETE_BY_ID = "DELETE FROM export_logs WHERE id = ?";

const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM export_logs WHERE id = ?";

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

// 정리 (Log Retention)
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

// 고급 통계 쿼리들
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

namespace PayloadTemplate {

const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS payload_templates (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name VARCHAR(100) NOT NULL UNIQUE,
            system_type VARCHAR(50) NOT NULL,
            description TEXT,
            template_json TEXT NOT NULL,
            is_active BOOLEAN DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

const std::string CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_payload_templates_system ON payload_templates(system_type);
        CREATE INDEX IF NOT EXISTS idx_payload_templates_active ON payload_templates(is_active);
    )";

const std::string FIND_ALL = R"(
        SELECT id, name, system_type, description, template_json, is_active,
               created_at, updated_at
        FROM payload_templates
        ORDER BY system_type, name
    )";

const std::string FIND_BY_ID = R"(
        SELECT id, name, system_type, description, template_json, is_active,
               created_at, updated_at
        FROM payload_templates
        WHERE id = ?
    )";

const std::string FIND_BY_NAME = R"(
        SELECT id, name, system_type, description, template_json, is_active,
               created_at, updated_at
        FROM payload_templates
        WHERE name = ?
    )";

const std::string FIND_BY_SYSTEM_TYPE = R"(
        SELECT id, name, system_type, description, template_json, is_active,
               created_at, updated_at
        FROM payload_templates
        WHERE system_type = ?
        ORDER BY name
    )";

const std::string FIND_ACTIVE = R"(
        SELECT id, name, system_type, description, template_json, is_active,
               created_at, updated_at
        FROM payload_templates
        WHERE is_active = 1
        ORDER BY system_type, name
    )";

const std::string INSERT = R"(
        INSERT INTO payload_templates (
            name, system_type, description, template_json, is_active
        ) VALUES (?, ?, ?, ?, ?)
    )";

const std::string UPDATE = R"(
        UPDATE payload_templates SET
            name = ?, system_type = ?, description = ?,
            template_json = ?, is_active = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

const std::string DELETE_BY_ID = "DELETE FROM payload_templates WHERE id = ?";

const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM payload_templates WHERE id = ?";

const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM payload_templates";

const std::string COUNT_BY_SYSTEM_TYPE = R"(
        SELECT system_type, COUNT(*) as count
        FROM payload_templates
        GROUP BY system_type
    )";

} // namespace PayloadTemplate

namespace ExportSchedule {

const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS export_schedules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            profile_id INTEGER,
            target_id INTEGER NOT NULL,
            schedule_name VARCHAR(100) NOT NULL,
            description TEXT,
            
            cron_expression VARCHAR(100) NOT NULL,
            timezone VARCHAR(50) DEFAULT 'UTC',
            data_range VARCHAR(20) DEFAULT 'day',
            lookback_periods INTEGER DEFAULT 1,
            
            is_enabled BOOLEAN DEFAULT 1,
            last_run_at DATETIME,
            last_status VARCHAR(20),
            next_run_at DATETIME,
            
            total_runs INTEGER DEFAULT 0,
            successful_runs INTEGER DEFAULT 0,
            failed_runs INTEGER DEFAULT 0,
            
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (profile_id) REFERENCES export_profiles(id) ON DELETE SET NULL,
            FOREIGN KEY (target_id) REFERENCES export_targets(id) ON DELETE CASCADE
        )
    )";

const std::string CREATE_INDEXES = R"(
        CREATE INDEX IF NOT EXISTS idx_export_schedules_enabled ON export_schedules(is_enabled);
        CREATE INDEX IF NOT EXISTS idx_export_schedules_next_run ON export_schedules(next_run_at);
        CREATE INDEX IF NOT EXISTS idx_export_schedules_target ON export_schedules(target_id);
    )";

const std::string FIND_ALL = R"(
        SELECT id, profile_id, target_id, schedule_name, description,
               cron_expression, timezone, data_range, lookback_periods,
               is_enabled, last_run_at, last_status, next_run_at,
               total_runs, successful_runs, failed_runs,
               created_at, updated_at
        FROM export_schedules
        ORDER BY schedule_name ASC
    )";

const std::string FIND_BY_ID = R"(
        SELECT id, profile_id, target_id, schedule_name, description,
               cron_expression, timezone, data_range, lookback_periods,
               is_enabled, last_run_at, last_status, next_run_at,
               total_runs, successful_runs, failed_runs,
               created_at, updated_at
        FROM export_schedules WHERE id = ?
    )";

const std::string FIND_ENABLED = R"(
        SELECT id, profile_id, target_id, schedule_name, description,
               cron_expression, timezone, data_range, lookback_periods,
               is_enabled, last_run_at, last_status, next_run_at,
               total_runs, successful_runs, failed_runs,
               created_at, updated_at
        FROM export_schedules 
        WHERE is_enabled = 1
        ORDER BY next_run_at ASC
    )";

const std::string FIND_BY_TARGET_ID = R"(
        SELECT id, profile_id, target_id, schedule_name, description,
               cron_expression, timezone, data_range, lookback_periods,
               is_enabled, last_run_at, last_status, next_run_at,
               total_runs, successful_runs, failed_runs,
               created_at, updated_at
        FROM export_schedules 
        WHERE target_id = ?
        ORDER BY schedule_name ASC
    )";

const std::string FIND_PENDING = R"(
        SELECT id, profile_id, target_id, schedule_name, description,
               cron_expression, timezone, data_range, lookback_periods,
               is_enabled, last_run_at, last_status, next_run_at,
               total_runs, successful_runs, failed_runs,
               created_at, updated_at
        FROM export_schedules 
        WHERE is_enabled = 1 
          AND next_run_at <= datetime('now')
        ORDER BY next_run_at ASC
    )";

const std::string INSERT = R"(
        INSERT INTO export_schedules (
            profile_id, target_id, schedule_name, description,
            cron_expression, timezone, data_range, lookback_periods,
            is_enabled, next_run_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

const std::string UPDATE = R"(
        UPDATE export_schedules SET
            profile_id = ?, target_id = ?, schedule_name = ?, description = ?,
            cron_expression = ?, timezone = ?, data_range = ?, lookback_periods = ?,
            is_enabled = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

const std::string UPDATE_RUN_STATUS = R"(
        UPDATE export_schedules SET
            last_run_at = ?,
            last_status = ?,
            next_run_at = ?,
            total_runs = total_runs + 1,
            successful_runs = successful_runs + ?,
            failed_runs = failed_runs + ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

const std::string DELETE_BY_ID = "DELETE FROM export_schedules WHERE id = ?";

const std::string EXISTS_BY_ID =
    "SELECT COUNT(*) as count FROM export_schedules WHERE id = ?";

const std::string COUNT_ENABLED =
    "SELECT COUNT(*) as count FROM export_schedules WHERE is_enabled = 1";

} // namespace ExportSchedule

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

#endif // EXTENDED_SQL_QUERIES_H