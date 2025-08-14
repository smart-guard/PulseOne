// ============================================================================
// backend/lib/database/queries/AlarmQueries.js
// C++ SQLQueries.h 패턴 - AlarmRule, AlarmOccurrence 네임스페이스
// 실제 데이터베이스 스키마와 100% 일치
// ============================================================================

/**
 * 알람 관련 SQL 쿼리들 (C++ SQLQueries.h와 동일한 구조)
 * DeviceQueries.js 패턴과 100% 일치
 */
const AlarmQueries = {
    
    // ========================================================================
    // 🚨 AlarmRule 관련 쿼리들 (실제 스키마 기반)
    // ========================================================================
    AlarmRule: {
        // 기본 CRUD 쿼리들
        FIND_ALL: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id, target_group,
                alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                deadband, rate_of_change, trigger_condition, condition_script, message_script,
                message_config, message_template, severity, priority, auto_acknowledge,
                acknowledge_timeout_min, auto_clear, suppression_rules, notification_enabled,
                notification_delay_sec, notification_repeat_interval_min, notification_channels,
                notification_recipients, is_enabled, is_latched, created_at, updated_at, created_by
            FROM alarm_rules
        `,
        
        FIND_BY_ID: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id, target_group,
                alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                deadband, rate_of_change, trigger_condition, condition_script, message_script,
                message_config, message_template, severity, priority, auto_acknowledge,
                acknowledge_timeout_min, auto_clear, suppression_rules, notification_enabled,
                notification_delay_sec, notification_repeat_interval_min, notification_channels,
                notification_recipients, is_enabled, is_latched, created_at, updated_at, created_by
            FROM alarm_rules 
            WHERE id = ?
        `,
        
        CREATE: `
            INSERT INTO alarm_rules (
                tenant_id, name, description, target_type, target_id, target_group,
                alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                deadband, rate_of_change, trigger_condition, condition_script, message_script,
                message_config, message_template, severity, priority, auto_acknowledge,
                acknowledge_timeout_min, auto_clear, suppression_rules, notification_enabled,
                notification_delay_sec, notification_repeat_interval_min, notification_channels,
                notification_recipients, is_enabled, is_latched, created_by
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        UPDATE: `
            UPDATE alarm_rules SET 
                name = ?, description = ?, target_type = ?, target_id = ?, target_group = ?,
                alarm_type = ?, high_high_limit = ?, high_limit = ?, low_limit = ?, low_low_limit = ?,
                deadband = ?, rate_of_change = ?, trigger_condition = ?, condition_script = ?, message_script = ?,
                message_config = ?, message_template = ?, severity = ?, priority = ?, auto_acknowledge = ?,
                acknowledge_timeout_min = ?, auto_clear = ?, suppression_rules = ?, notification_enabled = ?,
                notification_delay_sec = ?, notification_repeat_interval_min = ?, notification_channels = ?,
                notification_recipients = ?, is_enabled = ?, is_latched = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
        `,
        
        DELETE: `
            DELETE FROM alarm_rules WHERE id = ?
        `,
        
        EXISTS: `
            SELECT 1 FROM alarm_rules WHERE id = ? LIMIT 1
        `,
        
        // 특화 쿼리들 (C++ 메서드와 동일)
        FIND_BY_TARGET: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id, target_group,
                alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                deadband, rate_of_change, trigger_condition, condition_script, message_script,
                message_config, message_template, severity, priority, auto_acknowledge,
                acknowledge_timeout_min, auto_clear, suppression_rules, notification_enabled,
                notification_delay_sec, notification_repeat_interval_min, notification_channels,
                notification_recipients, is_enabled, is_latched, created_at, updated_at, created_by
            FROM alarm_rules 
            WHERE target_type = ? AND target_id = ?
        `,
        
        FIND_ENABLED: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id, target_group,
                alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                deadband, rate_of_change, trigger_condition, condition_script, message_script,
                message_config, message_template, severity, priority, auto_acknowledge,
                acknowledge_timeout_min, auto_clear, suppression_rules, notification_enabled,
                notification_delay_sec, notification_repeat_interval_min, notification_channels,
                notification_recipients, is_enabled, is_latched, created_at, updated_at, created_by
            FROM alarm_rules 
            WHERE is_enabled = 1
        `,
        
        FIND_BY_TYPE: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id, target_group,
                alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                deadband, rate_of_change, trigger_condition, condition_script, message_script,
                message_config, message_template, severity, priority, auto_acknowledge,
                acknowledge_timeout_min, auto_clear, suppression_rules, notification_enabled,
                notification_delay_sec, notification_repeat_interval_min, notification_channels,
                notification_recipients, is_enabled, is_latched, created_at, updated_at, created_by
            FROM alarm_rules 
            WHERE alarm_type = ?
        `,
        
        FIND_BY_SEVERITY: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id, target_group,
                alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                deadband, rate_of_change, trigger_condition, condition_script, message_script,
                message_config, message_template, severity, priority, auto_acknowledge,
                acknowledge_timeout_min, auto_clear, suppression_rules, notification_enabled,
                notification_delay_sec, notification_repeat_interval_min, notification_channels,
                notification_recipients, is_enabled, is_latched, created_at, updated_at, created_by
            FROM alarm_rules 
            WHERE severity = ?
        `,
        
        COUNT_BY_TENANT: `
            SELECT COUNT(*) as count FROM alarm_rules WHERE tenant_id = ?
        `,
        
        COUNT_ENABLED: `
            SELECT COUNT(*) as count FROM alarm_rules WHERE is_enabled = 1
        `,
        
        // 조건부 쿼리들
        CONDITIONS: {
            TENANT_ID: ` AND tenant_id = ?`,
            ENABLED: ` AND is_enabled = 1`,
            DISABLED: ` AND is_enabled = 0`,
            ORDER_BY_NAME: ` ORDER BY name ASC`,
            ORDER_BY_CREATED: ` ORDER BY created_at DESC`,
            ORDER_BY_SEVERITY: ` ORDER BY CASE severity WHEN 'critical' THEN 1 WHEN 'high' THEN 2 WHEN 'medium' THEN 3 WHEN 'low' THEN 4 END ASC`
        }
    },
    
    // ========================================================================
    // 🔔 AlarmOccurrence 관련 쿼리들 (실제 스키마 기반)
    // ========================================================================
    AlarmOccurrence: {
        // 기본 CRUD 쿼리들
        FIND_ALL: `
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at, device_id, point_id
            FROM alarm_occurrences
        `,
        
        FIND_BY_ID: `
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at, device_id, point_id
            FROM alarm_occurrences 
            WHERE id = ?
        `,
        
        CREATE: `
            INSERT INTO alarm_occurrences (
                rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, context_data, source_name, location, device_id, point_id
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        UPDATE: `
            UPDATE alarm_occurrences SET 
                state = ?, acknowledged_time = ?, acknowledged_by = ?, acknowledge_comment = ?,
                cleared_time = ?, cleared_value = ?, clear_comment = ?, notification_sent = ?,
                notification_time = ?, notification_count = ?, notification_result = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
        `,
        
        DELETE: `
            DELETE FROM alarm_occurrences WHERE id = ?
        `,
        
        EXISTS: `
            SELECT 1 FROM alarm_occurrences WHERE id = ? LIMIT 1
        `,
        
        // 특화 쿼리들 (C++ 메서드와 동일)
        FIND_ACTIVE: `
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at, device_id, point_id
            FROM alarm_occurrences 
            WHERE state = 'active'
            ORDER BY occurrence_time DESC
        `,
        
        FIND_BY_RULE: `
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at, device_id, point_id
            FROM alarm_occurrences 
            WHERE rule_id = ?
            ORDER BY occurrence_time DESC
        `,
        
        FIND_BY_DEVICE: `
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at, device_id, point_id
            FROM alarm_occurrences 
            WHERE device_id = ?
            ORDER BY occurrence_time DESC
        `,
        
        FIND_BY_POINT: `
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at, device_id, point_id
            FROM alarm_occurrences 
            WHERE point_id = ?
            ORDER BY occurrence_time DESC
        `,
        
        FIND_UNACKNOWLEDGED: `
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at, device_id, point_id
            FROM alarm_occurrences 
            WHERE acknowledged_time IS NULL AND state = 'active'
            ORDER BY occurrence_time DESC
        `,
        
        FIND_BY_SEVERITY: `
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at, device_id, point_id
            FROM alarm_occurrences 
            WHERE severity = ?
            ORDER BY occurrence_time DESC
        `,
        
        FIND_RECENT: `
            SELECT 
                id, rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at, device_id, point_id
            FROM alarm_occurrences 
            WHERE occurrence_time >= datetime('now', '-1 day')
            ORDER BY occurrence_time DESC
        `,
        
        // 상태 변경 쿼리들 (C++ 메서드와 동일)
        ACKNOWLEDGE: `
            UPDATE alarm_occurrences SET 
                acknowledged_time = CURRENT_TIMESTAMP, 
                acknowledged_by = ?, 
                acknowledge_comment = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND acknowledged_time IS NULL
        `,
        
        CLEAR: `
            UPDATE alarm_occurrences SET 
                state = 'cleared', 
                cleared_time = CURRENT_TIMESTAMP, 
                cleared_value = ?, 
                clear_comment = ?, 
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
        `,
        
        // 통계 쿼리들
        COUNT_ACTIVE: `
            SELECT COUNT(*) as count FROM alarm_occurrences WHERE state = 'active'
        `,
        
        COUNT_BY_SEVERITY: `
            SELECT severity, COUNT(*) as count 
            FROM alarm_occurrences 
            WHERE state = 'active' 
            GROUP BY severity
        `,
        
        COUNT_BY_DEVICE: `
            SELECT device_id, COUNT(*) as count 
            FROM alarm_occurrences 
            WHERE state = 'active' AND device_id IS NOT NULL
            GROUP BY device_id
        `,
        
        COUNT_UNACKNOWLEDGED: `
            SELECT COUNT(*) as count 
            FROM alarm_occurrences 
            WHERE acknowledged_time IS NULL AND state = 'active'
        `,
        
        COUNT_BY_TENANT: `
            SELECT COUNT(*) as count FROM alarm_occurrences WHERE tenant_id = ?
        `,
        
        // 조건부 쿼리들
        CONDITIONS: {
            TENANT_ID: ` AND tenant_id = ?`,
            ACTIVE: ` AND state = 'active'`,
            ACKNOWLEDGED: ` AND acknowledged_time IS NOT NULL`,
            UNACKNOWLEDGED: ` AND acknowledged_time IS NULL`,
            ORDER_BY_TIME: ` ORDER BY occurrence_time DESC`,
            ORDER_BY_SEVERITY: ` ORDER BY CASE severity WHEN 'critical' THEN 1 WHEN 'high' THEN 2 WHEN 'medium' THEN 3 WHEN 'low' THEN 4 END ASC`,
            LIMIT: ` LIMIT ?`,
            OFFSET: ` OFFSET ?`
        }
    },
    
    // ========================================================================
    // 🔗 JOIN 쿼리들 (실제 스키마 기반 복합 쿼리)
    // ========================================================================
    Joins: {
        // 활성 알람 + 규칙 정보
        ACTIVE_WITH_RULES: `
            SELECT 
                ao.id, ao.rule_id, ao.tenant_id, ao.device_id, ao.point_id,
                ao.occurrence_time, ao.trigger_value, ao.trigger_condition,
                ao.alarm_message, ao.severity, ao.state, ao.acknowledged_time,
                ao.acknowledged_by, ao.acknowledge_comment, ao.source_name, ao.location,
                ar.name as rule_name, ar.description as rule_description,
                ar.target_type, ar.target_id, ar.alarm_type
            FROM alarm_occurrences ao
            INNER JOIN alarm_rules ar ON ao.rule_id = ar.id
            WHERE ao.state IN ('active', 'acknowledged')
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 알람 이력 + 규칙 정보
        HISTORY_WITH_RULES: `
            SELECT 
                ao.id, ao.rule_id, ao.tenant_id, ao.device_id, ao.point_id,
                ao.occurrence_time, ao.trigger_value, ao.trigger_condition,
                ao.alarm_message, ao.severity, ao.state, ao.acknowledged_time,
                ao.acknowledged_by, ao.acknowledge_comment, ao.cleared_time,
                ao.cleared_value, ao.clear_comment, ao.source_name, ao.location, ao.created_at,
                ar.name as rule_name, ar.description as rule_description,
                ar.target_type, ar.target_id, ar.alarm_type
            FROM alarm_occurrences ao
            INNER JOIN alarm_rules ar ON ao.rule_id = ar.id
            ORDER BY ao.occurrence_time DESC
            LIMIT ? OFFSET ?
        `,
        
        // 알람 발생 + 디바이스 정보
        OCCURRENCE_WITH_DEVICE: `
            SELECT 
                ao.id, ao.rule_id, ao.device_id, ao.point_id, ao.occurrence_time,
                ao.state, ao.trigger_value, ao.alarm_message, ao.severity,
                ao.acknowledged_time, ao.acknowledged_by, ao.cleared_time,
                d.name as device_name, d.location as device_location, d.protocol_type, d.ip_address
            FROM alarm_occurrences ao
            LEFT JOIN devices d ON ao.device_id = d.id
        `,
        
        // 알람 규칙 + 대상 정보
        RULE_WITH_TARGET: `
            SELECT 
                ar.id, ar.name, ar.description, ar.target_type, ar.target_id,
                ar.alarm_type, ar.severity, ar.is_enabled,
                CASE ar.target_type
                    WHEN 'device' THEN d.name
                    WHEN 'data_point' THEN dp.name
                    WHEN 'virtual_point' THEN vp.name
                    ELSE 'Unknown'
                END as target_name
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            LEFT JOIN virtual_points vp ON ar.target_type = 'virtual_point' AND ar.target_id = vp.id
        `,
        
        // 디바이스별 알람 요약
        DEVICE_ALARM_SUMMARY: `
            SELECT 
                d.id as device_id, d.name as device_name,
                COUNT(*) as total_alarms,
                COUNT(CASE WHEN ao.state = 'active' THEN 1 END) as active_alarms,
                COUNT(CASE WHEN ao.acknowledged_time IS NULL AND ao.state = 'active' THEN 1 END) as unacknowledged_alarms,
                MAX(ao.occurrence_time) as latest_alarm_time
            FROM devices d
            LEFT JOIN alarm_occurrences ao ON d.id = ao.device_id
            WHERE ao.occurrence_time >= datetime('now', '-24 hours')
            GROUP BY d.id, d.name
            HAVING total_alarms > 0
            ORDER BY active_alarms DESC, latest_alarm_time DESC
        `
    }
};

module.exports = AlarmQueries;