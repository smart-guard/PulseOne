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
    
    // ==========================================================================
    // 테이블 생성 및 관리
    // ==========================================================================
    
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
            
            -- 성능 최적화 인덱스들
            INDEX idx_alarm_occurrences_state (state),
            INDEX idx_alarm_occurrences_tenant (tenant_id),
            INDEX idx_alarm_occurrences_rule (rule_id),
            INDEX idx_alarm_occurrences_severity (severity),
            INDEX idx_alarm_occurrences_occurrence_time (occurrence_time),
            INDEX idx_alarm_occurrences_active_tenant (state, tenant_id) WHERE state = 'active',
            INDEX idx_alarm_occurrences_notification (notification_sent, state) WHERE state = 'active',
            INDEX idx_alarm_occurrences_composite (tenant_id, state, severity, occurrence_time)
        )
    )";
    
    // ==========================================================================
    // 기본 CRUD 쿼리들 (ScriptLibraryRepository 패턴)
    // ==========================================================================
    
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
            rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE = R"(
        UPDATE alarm_occurrences SET
            rule_id = ?, tenant_id = ?, occurrence_time = ?, trigger_value = ?,
            trigger_condition = ?, alarm_message = ?, severity = ?, state = ?,
            acknowledged_time = ?, acknowledged_by = ?, acknowledge_comment = ?,
            cleared_time = ?, cleared_value = ?, clear_comment = ?,
            notification_sent = ?, notification_time = ?, notification_count = ?,
            notification_result = ?, context_data = ?, source_name = ?, location = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM alarm_occurrences WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM alarm_occurrences WHERE id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM alarm_occurrences";
    const std::string FIND_MAX_ID = "SELECT MAX(id) as max_id FROM alarm_occurrences";
    // ==========================================================================
    // 특화 조회 쿼리들 (AlarmOccurrence 전용)
    // ==========================================================================
    
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
    
    const std::string FIND_BY_TENANT = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE tenant_id = ?
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_BY_SEVERITY = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE severity = ?
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_RECENT = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        ORDER BY occurrence_time DESC
        LIMIT ?
    )";
    
    // ==========================================================================
    // 알람 상태 관리 쿼리들
    // ==========================================================================
    
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
    
    const std::string SUPPRESS = R"(
        UPDATE alarm_occurrences SET 
            state = 'suppressed',
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string UPDATE_NOTIFICATION_STATUS = R"(
        UPDATE alarm_occurrences SET 
            notification_sent = ?, 
            notification_time = CURRENT_TIMESTAMP,
            notification_count = notification_count + 1,
            notification_result = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // ==========================================================================
    // 알람 통계 및 분석 쿼리들
    // ==========================================================================
    
    const std::string GET_STATISTICS_BY_TENANT = R"(
        SELECT 
            COUNT(*) as total,
            COUNT(CASE WHEN state = 'active' THEN 1 END) as active,
            COUNT(CASE WHEN state = 'acknowledged' THEN 1 END) as acknowledged,
            COUNT(CASE WHEN state = 'cleared' THEN 1 END) as cleared,
            COUNT(CASE WHEN state = 'suppressed' THEN 1 END) as suppressed
        FROM alarm_occurrences 
        WHERE tenant_id = ?
    )";
    
    const std::string GET_ACTIVE_ALARMS_BY_SEVERITY = R"(
        SELECT severity, COUNT(*) as count 
        FROM alarm_occurrences 
        WHERE tenant_id = ? AND state = 'active' 
        GROUP BY severity
        ORDER BY 
            CASE severity 
                WHEN 'CRITICAL' THEN 1 
                WHEN 'HIGH' THEN 2 
                WHEN 'MEDIUM' THEN 3 
                WHEN 'LOW' THEN 4 
                ELSE 5 
            END
    )";
    
    const std::string GET_ALARM_TREND_BY_HOURS = R"(
        SELECT 
            strftime('%Y-%m-%d %H:00:00', occurrence_time) as hour,
            COUNT(*) as count,
            COUNT(CASE WHEN state = 'active' THEN 1 END) as active_count
        FROM alarm_occurrences 
        WHERE occurrence_time >= datetime('now', '-24 hours')
        AND (tenant_id = ? OR ? = 0)
        GROUP BY strftime('%Y-%m-%d %H:00:00', occurrence_time)
        ORDER BY hour DESC
    )";
    
    const std::string GET_ALARM_TREND_BY_DAYS = R"(
        SELECT 
            DATE(occurrence_time) as alarm_date,
            COUNT(*) as count,
            COUNT(CASE WHEN state = 'active' THEN 1 END) as active_count
        FROM alarm_occurrences 
        WHERE occurrence_time >= date('now', '-' || ? || ' days')
        AND (tenant_id = ? OR ? = 0)
        GROUP BY DATE(occurrence_time)
        ORDER BY alarm_date DESC
    )";
    
    const std::string GET_TOP_ALARM_SOURCES = R"(
        SELECT 
            source_name, 
            COUNT(*) as count,
            COUNT(CASE WHEN state = 'active' THEN 1 END) as active_count
        FROM alarm_occurrences 
        WHERE tenant_id = ? 
        AND occurrence_time >= datetime('now', '-30 days')
        AND source_name IS NOT NULL
        GROUP BY source_name
        ORDER BY count DESC
        LIMIT ?
    )";
    
    // ==========================================================================
    // 유지보수 및 정리 쿼리들
    // ==========================================================================
    
    const std::string CLEANUP_OLD_CLEARED_ALARMS = R"(
        DELETE FROM alarm_occurrences 
        WHERE state = 'cleared' 
        AND cleared_time < datetime('now', '-' || ? || ' days')
    )";
    
    const std::string ARCHIVE_OLD_ALARMS = R"(
        INSERT INTO alarm_occurrences_archive 
        SELECT * FROM alarm_occurrences 
        WHERE state IN ('cleared', 'acknowledged') 
        AND occurrence_time < datetime('now', '-' || ? || ' days')
    )";
    
    const std::string DELETE_ARCHIVED_ALARMS = R"(
        DELETE FROM alarm_occurrences 
        WHERE state IN ('cleared', 'acknowledged') 
        AND occurrence_time < datetime('now', '-' || ? || ' days')
    )";
    
    const std::string GET_OLD_ALARMS_COUNT = R"(
        SELECT COUNT(*) as count 
        FROM alarm_occurrences 
        WHERE state = 'cleared' 
        AND cleared_time < datetime('now', '-' || ? || ' days')
    )";
    
    // ==========================================================================
    // 알림 관련 쿼리들
    // ==========================================================================
    
    const std::string FIND_PENDING_NOTIFICATIONS = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE notification_sent = 0 AND state = 'active'
        ORDER BY occurrence_time ASC
    )";
    
    const std::string FIND_RETRY_NOTIFICATIONS = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE notification_sent = 0 
        AND notification_count < ? 
        AND notification_time < datetime('now', '-' || ? || ' minutes')
        AND state = 'active'
        ORDER BY occurrence_time ASC
    )";
    
    // ==========================================================================
    // 고급 조회 쿼리들
    // ==========================================================================
    
    const std::string FIND_BY_TIME_RANGE = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE occurrence_time >= ? AND occurrence_time <= ?
        ORDER BY occurrence_time DESC
    )";
    
    const std::string FIND_ESCALATED_ALARMS = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE state = 'active'
        AND occurrence_time < datetime('now', '-' || ? || ' minutes')
        AND acknowledged_time IS NULL
        ORDER BY occurrence_time ASC
    )";
    
    const std::string FIND_UNACKNOWLEDGED_CRITICAL = R"(
        SELECT 
            id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
            alarm_message, severity, state, acknowledged_time, acknowledged_by,
            acknowledge_comment, cleared_time, cleared_value, clear_comment,
            notification_sent, notification_time, notification_count, notification_result,
            context_data, source_name, location, created_at, updated_at
        FROM alarm_occurrences 
        WHERE state = 'active'
        AND severity = 'CRITICAL'
        AND acknowledged_time IS NULL
        ORDER BY occurrence_time ASC
    )";
    
    // ==========================================================================
    // 성능 최적화 쿼리들
    // ==========================================================================
    
    const std::string VACUUM_TABLE = "VACUUM alarm_occurrences";
    
    const std::string ANALYZE_TABLE = "ANALYZE alarm_occurrences";
    
    const std::string REINDEX_TABLE = "REINDEX alarm_occurrences";
    
    const std::string CHECK_TABLE_SIZE = R"(
        SELECT 
            COUNT(*) as total_rows,
            COUNT(CASE WHEN state = 'active' THEN 1 END) as active_rows,
            COUNT(CASE WHEN state = 'cleared' THEN 1 END) as cleared_rows,
            COUNT(CASE WHEN occurrence_time >= date('now', '-30 days') THEN 1 END) as recent_rows
        FROM alarm_occurrences
    )";

} // namespace Occurrence

namespace AlarmRule {
    
    // 🔥🔥🔥 FIND_ALL - 모든 알람 규칙 조회 (우선순위, 중요도 순)
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
        ORDER BY priority DESC, severity, name
    )";
    
    // 🔥🔥🔥 FIND_BY_ID - 특정 알람 규칙 조회
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
    
    // 🔥🔥🔥 FIND_BY_TARGET - 특정 대상에 대한 알람 규칙들
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
        WHERE target_type = ? AND target_id = ?
        ORDER BY priority DESC, severity
    )";
    
    // 🔥🔥🔥 FIND_BY_TENANT - 테넌트별 알람 규칙들
    const std::string FIND_BY_TENANT = R"(
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
        WHERE tenant_id = ?
        ORDER BY priority DESC, severity, name
    )";
    
    // 🔥🔥🔥 FIND_ENABLED - 활성화된 알람 규칙들만
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
        ORDER BY priority DESC, severity
    )";
    
    // 🔥🔥🔥 FIND_BY_SEVERITY - 중요도별 알람 규칙들
    const std::string FIND_BY_SEVERITY = R"(
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
        WHERE severity = ?
        ORDER BY priority DESC, name
    )";
    
    // 🔥🔥🔥 FIND_BY_ALARM_TYPE - 알람 타입별 (analog, digital, script)
    const std::string FIND_BY_ALARM_TYPE = R"(
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
        WHERE alarm_type = ?
        ORDER BY priority DESC, severity
    )";
    
    // 🔥🔥🔥 INSERT - 새 알람 규칙 생성
    const std::string INSERT = R"(
        INSERT INTO alarm_rules (
            tenant_id, name, description, target_type, target_id, target_group,
            alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
            deadband, rate_of_change, trigger_condition, condition_script,
            message_script, message_config, message_template, severity, priority,
            auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
            notification_enabled, notification_delay_sec, notification_repeat_interval_min,
            notification_channels, notification_recipients, is_enabled, is_latched,
            created_by, created_at, updated_at
        ) VALUES (
            ?, ?, ?, ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?, ?,
            ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP
        )
    )";
    
    // 🔥🔥🔥 UPDATE - 알람 규칙 업데이트
    const std::string UPDATE = R"(
        UPDATE alarm_rules SET 
            tenant_id = ?, name = ?, description = ?, target_type = ?, target_id = ?, target_group = ?,
            alarm_type = ?, high_high_limit = ?, high_limit = ?, low_limit = ?, low_low_limit = ?,
            deadband = ?, rate_of_change = ?, trigger_condition = ?, condition_script = ?,
            message_script = ?, message_config = ?, message_template = ?, severity = ?, priority = ?,
            auto_acknowledge = ?, acknowledge_timeout_min = ?, auto_clear = ?, suppression_rules = ?,
            notification_enabled = ?, notification_delay_sec = ?, notification_repeat_interval_min = ?,
            notification_channels = ?, notification_recipients = ?, is_enabled = ?, is_latched = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // 🔥🔥🔥 UPDATE_STATUS - 알람 규칙 활성화/비활성화
    const std::string UPDATE_STATUS = R"(
        UPDATE alarm_rules SET 
            is_enabled = ?, updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // 🔥🔥🔥 기본 CRUD 및 유틸리티
    const std::string DELETE_BY_ID = "DELETE FROM alarm_rules WHERE id = ?";
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM alarm_rules WHERE id = ?";
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM alarm_rules";
    const std::string COUNT_ENABLED = "SELECT COUNT(*) as count FROM alarm_rules WHERE is_enabled = 1";
    const std::string COUNT_BY_SEVERITY = "SELECT COUNT(*) as count FROM alarm_rules WHERE severity = ?";
    const std::string COUNT_BY_TARGET = "SELECT COUNT(*) as count FROM alarm_rules WHERE target_type = ? AND target_id = ?";
    const std::string GET_LAST_INSERT_ID = "SELECT last_insert_rowid() as id";
    
    // 🔥🔥🔥 통계 쿼리들
    const std::string GET_SEVERITY_DISTRIBUTION = R"(
        SELECT severity, COUNT(*) as count 
        FROM alarm_rules 
        GROUP BY severity
        ORDER BY 
            CASE severity 
                WHEN 'critical' THEN 1
                WHEN 'high' THEN 2  
                WHEN 'medium' THEN 3
                WHEN 'low' THEN 4
                WHEN 'info' THEN 5
                ELSE 6
            END
    )";
    
    const std::string GET_TYPE_DISTRIBUTION = R"(
        SELECT alarm_type, COUNT(*) as count 
        FROM alarm_rules 
        GROUP BY alarm_type
        ORDER BY count DESC
    )";
    
    // 🔥🔥🔥 CREATE_TABLE - alarm_rules 테이블 생성
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS alarm_rules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL,
            name VARCHAR(100) NOT NULL,
            description TEXT,
            
            -- 대상 정보
            target_type VARCHAR(20) NOT NULL,  -- 'data_point', 'virtual_point', 'group'
            target_id INTEGER,
            target_group VARCHAR(100),
            
            -- 알람 타입
            alarm_type VARCHAR(20) NOT NULL,  -- 'analog', 'digital', 'script'
            
            -- 아날로그 알람 설정
            high_high_limit REAL,
            high_limit REAL,
            low_limit REAL,
            low_low_limit REAL,
            deadband REAL DEFAULT 0,
            rate_of_change REAL DEFAULT 0,
            
            -- 디지털 알람 설정
            trigger_condition VARCHAR(20),  -- 'on_true', 'on_false', 'on_change', 'on_rising', 'on_falling'
            
            -- 스크립트 기반 알람
            condition_script TEXT,
            message_script TEXT,
            
            -- 메시지 커스터마이징
            message_config TEXT,  -- JSON 형태
            message_template TEXT,
            
            -- 우선순위
            severity VARCHAR(20) DEFAULT 'medium',  -- 'critical', 'high', 'medium', 'low', 'info'
            priority INTEGER DEFAULT 100,
            
            -- 자동 처리
            auto_acknowledge INTEGER DEFAULT 0,
            acknowledge_timeout_min INTEGER DEFAULT 0,
            auto_clear INTEGER DEFAULT 1,
            
            -- 억제 규칙
            suppression_rules TEXT,  -- JSON 형태
            
            -- 알림 설정
            notification_enabled INTEGER DEFAULT 1,
            notification_delay_sec INTEGER DEFAULT 0,
            notification_repeat_interval_min INTEGER DEFAULT 0,
            notification_channels TEXT,  -- JSON 배열
            notification_recipients TEXT,  -- JSON 배열
            
            -- 상태
            is_enabled INTEGER DEFAULT 1,
            is_latched INTEGER DEFAULT 0,
            
            -- 타임스탬프
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            created_by INTEGER,
            
            FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
            FOREIGN KEY (created_by) REFERENCES users(id)
        )
    )";
    
} // namespace AlarmRule

} // namespace Alarm

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

    const std::string FIND_BY_NAME = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE (tenant_id = ? OR is_system = 1) AND name = ?
    )";
    
    const std::string FIND_TEMPLATES = R"(
        SELECT id, name, display_name, description, category,
               script_code, parameters, return_type, example_usage
        FROM script_library 
        WHERE is_template = 1
        ORDER BY category, name
    )";
    
    const std::string FIND_TEMPLATE_BY_ID = R"(
        SELECT id, name, display_name, description, category,
               script_code, parameters, return_type, example_usage
        FROM script_library 
        WHERE is_template = 1 AND id = ?
    )";
    
    const std::string FIND_TEMPLATES_BY_CATEGORY = R"(
        SELECT id, name, display_name, description, category,
               script_code, parameters, return_type, example_usage
        FROM script_library 
        WHERE is_template = 1 AND category = ?
        ORDER BY name
    )";
    
    const std::string COUNT_ALL_SCRIPTS = R"(
        SELECT COUNT(*) as total_scripts 
        FROM script_library
    )";
    
    const std::string COUNT_BY_TENANT = R"(
        SELECT COUNT(*) as total_scripts 
        FROM script_library 
        WHERE tenant_id = ?
    )";
    
    const std::string GROUP_BY_CATEGORY = R"(
        SELECT category, COUNT(*) as count 
        FROM script_library 
        GROUP BY category
    )";
    
    const std::string GROUP_BY_CATEGORY_AND_TENANT = R"(
        SELECT category, COUNT(*) as count 
        FROM script_library 
        WHERE tenant_id = ? 
        GROUP BY category
    )";
    
    const std::string USAGE_STATISTICS = R"(
        SELECT SUM(usage_count) as total_usage, 
               AVG(usage_count) as avg_usage 
        FROM script_library
    )";
    
    const std::string USAGE_STATISTICS_BY_TENANT = R"(
        SELECT SUM(usage_count) as total_usage, 
               AVG(usage_count) as avg_usage 
        FROM script_library 
        WHERE tenant_id = ?
    )";

        const std::string FIND_BY_IDS = R"(
        SELECT 
            id, tenant_id, name, display_name, description, category,
            script_code, parameters, return_type, tags, example_usage,
            is_system, is_template, usage_count, rating, version,
            author, license, dependencies, created_at, updated_at
        FROM script_library 
        WHERE id IN (%IN_CLAUSE%)
        ORDER BY name
    )";
    
    const std::string DELETE_BY_IDS = R"(
        DELETE FROM script_library 
        WHERE id IN (%IN_CLAUSE%)
    )";

} // namespace ScriptLibrary

namespace VirtualPoint {
    
    const std::string CREATE_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_points (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tenant_id INTEGER NOT NULL DEFAULT 0,
            site_id INTEGER,
            device_id INTEGER,
            
            -- 가상포인트 기본 정보
            name VARCHAR(100) NOT NULL,
            description TEXT,
            formula TEXT NOT NULL,
            data_type VARCHAR(20) NOT NULL DEFAULT 'float',
            unit VARCHAR(20),
            
            -- 계산 설정
            calculation_interval INTEGER DEFAULT 1000,
            calculation_trigger VARCHAR(20) DEFAULT 'timer',
            cache_duration_ms INTEGER DEFAULT 5000,
            is_enabled INTEGER DEFAULT 1,
            
            -- 메타데이터
            category VARCHAR(50),
            tags TEXT DEFAULT '',
            scope_type VARCHAR(20) NOT NULL DEFAULT 'tenant',
            
            -- 실행 통계
            execution_count INTEGER DEFAULT 0,
            last_value REAL DEFAULT 0.0,
            last_error TEXT DEFAULT '',
            avg_execution_time_ms REAL DEFAULT 0.0,
            
            -- 생성/수정 정보
            created_by VARCHAR(100) DEFAULT 'system',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            -- 제약조건
            CONSTRAINT chk_scope_type CHECK (scope_type IN ('tenant', 'site', 'device')),
            CONSTRAINT chk_calculation_trigger CHECK (calculation_trigger IN ('timer', 'event', 'manual')),
            
            -- 인덱스
            INDEX idx_virtual_points_tenant (tenant_id),
            INDEX idx_virtual_points_enabled (is_enabled),
            INDEX idx_virtual_points_scope (scope_type, tenant_id)
        )
    )";
    
    const std::string FIND_ALL = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, cache_duration_ms, is_enabled,
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
            calculation_interval, calculation_trigger, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE id = ?
    )";
    
    const std::string INSERT = R"(
        INSERT INTO virtual_points (
            tenant_id, site_id, device_id, name, description, formula,
            data_type, unit, calculation_interval, calculation_trigger, cache_duration_ms,
            is_enabled, category, tags, scope_type, created_by
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    const std::string UPDATE_BY_ID = R"(
        UPDATE virtual_points SET 
            tenant_id = ?, site_id = ?, device_id = ?, name = ?, 
            description = ?, formula = ?, data_type = ?, unit = ?,
            calculation_interval = ?, calculation_trigger = ?, cache_duration_ms = ?,
            is_enabled = ?, category = ?, tags = ?, scope_type = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    const std::string DELETE_BY_ID = "DELETE FROM virtual_points WHERE id = ?";
    
    const std::string EXISTS_BY_ID = "SELECT COUNT(*) as count FROM virtual_points WHERE id = ?";
    
    const std::string COUNT_ALL = "SELECT COUNT(*) as count FROM virtual_points";
    
    // =======================================================================
    // VirtualPoint 특화 쿼리들
    // =======================================================================
    
    const std::string FIND_BY_TENANT = R"(
        SELECT 
            id, tenant_id, site_id, device_id,
            name, description, formula, data_type, unit,
            calculation_interval, calculation_trigger, cache_duration_ms, is_enabled,
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
            calculation_interval, calculation_trigger, cache_duration_ms, is_enabled,
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
            calculation_interval, calculation_trigger, cache_duration_ms, is_enabled,
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
            calculation_interval, calculation_trigger, cache_duration_ms, is_enabled,
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
            calculation_interval, calculation_trigger, cache_duration_ms, is_enabled,
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
            calculation_interval, calculation_trigger, cache_duration_ms, is_enabled,
            category, tags, scope_type,
            execution_count, last_value, last_error, avg_execution_time_ms,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE calculation_trigger = ?
        ORDER BY calculation_interval, name
    )";
    
    // =======================================================================
    // 실행 통계 업데이트 쿼리들
    // =======================================================================
    
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
    
    const std::string RESET_EXECUTION_STATS = R"(
        UPDATE virtual_points SET 
            execution_count = 0,
            last_value = 0.0,
            last_error = '',
            avg_execution_time_ms = 0.0,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?
    )";
    
    // =======================================================================
    // 가상포인트 입력 매핑 테이블 (VirtualPoint 의존성 관리용)
    // =======================================================================
    
    const std::string CREATE_INPUTS_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_point_inputs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            virtual_point_id INTEGER NOT NULL,
            variable_name VARCHAR(50) NOT NULL,
            source_type VARCHAR(20) NOT NULL,
            source_id INTEGER,
            expression TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
            CONSTRAINT chk_source_type CHECK (source_type IN ('data_point', 'virtual_point', 'constant')),
            
            INDEX idx_vp_inputs_virtual_point (virtual_point_id),
            INDEX idx_vp_inputs_source (source_type, source_id)
        )
    )";
    
    const std::string FIND_INPUTS_BY_VP_ID = R"(
        SELECT 
            id, variable_name, source_type, source_id, expression
        FROM virtual_point_inputs 
        WHERE virtual_point_id = ?
        ORDER BY variable_name
    )";
    
    const std::string INSERT_INPUT = R"(
        INSERT INTO virtual_point_inputs (
            virtual_point_id, variable_name, source_type, source_id, expression
        ) VALUES (?, ?, ?, ?, ?)
    )";
    
    const std::string DELETE_INPUTS_BY_VP_ID = R"(
        DELETE FROM virtual_point_inputs WHERE virtual_point_id = ?
    )";
    
    // =======================================================================
    // 실행 로그 테이블 (디버깅 및 모니터링용)
    // =======================================================================
    
    const std::string CREATE_EXECUTION_LOG_TABLE = R"(
        CREATE TABLE IF NOT EXISTS virtual_point_execution_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            virtual_point_id INTEGER NOT NULL,
            calculated_value REAL,
            execution_time_ms INTEGER,
            status VARCHAR(20) DEFAULT 'success',
            error_message TEXT,
            executed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            
            FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE,
            
            INDEX idx_vp_log_virtual_point (virtual_point_id),
            INDEX idx_vp_log_executed_at (executed_at)
        )
    )";
    
    const std::string INSERT_EXECUTION_LOG = R"(
        INSERT INTO virtual_point_execution_log (
            virtual_point_id, calculated_value, execution_time_ms, status, error_message
        ) VALUES (?, ?, ?, ?, ?)
    )";
    
    const std::string FIND_EXECUTION_LOG = R"(
        SELECT 
            id, virtual_point_id, calculated_value, execution_time_ms, 
            status, error_message, executed_at
        FROM virtual_point_execution_log 
        WHERE virtual_point_id = ?
        ORDER BY executed_at DESC
        LIMIT ?
    )";
    
    const std::string CLEANUP_OLD_LOGS = R"(
        DELETE FROM virtual_point_execution_log 
        WHERE executed_at < datetime('now', '-7 days')
    )";

        // 벌크 조회용 쿼리 (%IN_CLAUSE%는 RepositoryHelpers::buildInClause()로 치환)
    const std::string FIND_BY_IDS = R"(
        SELECT 
            id, tenant_id, site_id, device_id, name, description, formula,
            data_type, unit, calculation_interval, calculation_trigger,
            cache_duration_ms, is_enabled, category, tags, scope_type,
            execution_count, last_value, avg_execution_time_ms, last_error,
            created_by, created_at, updated_at
        FROM virtual_points 
        WHERE id IN (%IN_CLAUSE%) 
        ORDER BY id
    )";
    
    // 벌크 삭제용 쿼리
    const std::string DELETE_BY_IDS = R"(
        DELETE FROM virtual_points 
        WHERE id IN (%IN_CLAUSE%)
    )";

    
    

    


    
    
} // namespace VirtualPoint

} // namespace SQL
} // namespace Database  
} // namespace PulseOne

#endif // EXTENDED_SQL_QUERIES_H