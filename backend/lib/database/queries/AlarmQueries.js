// ============================================================================
// backend/lib/database/queries/AlarmQueries.js
// C++ SQLQueries.h 패턴 - AlarmRule, AlarmOccurrence 네임스페이스
// ============================================================================

/**
 * 알람 관련 SQL 쿼리들 (C++ SQLQueries.h와 동일한 구조)
 * DeviceQueries.js 패턴과 100% 일치
 */
const AlarmQueries = {
    
    // ========================================================================
    // 🚨 AlarmRule 관련 쿼리들 (C++ AlarmRule 네임스페이스와 동일)
    // ========================================================================
    AlarmRule: {
        // 기본 CRUD 쿼리들
        FIND_ALL: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id,
                alarm_type, severity, priority, is_enabled, auto_clear,
                high_limit, low_limit, deadband, digital_trigger,
                condition_script, message_script, suppress_duration,
                escalation_rules, tags, created_by, created_at, updated_at
            FROM alarm_rules
        `,
        
        FIND_BY_ID: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id,
                alarm_type, severity, priority, is_enabled, auto_clear,
                high_limit, low_limit, deadband, digital_trigger,
                condition_script, message_script, suppress_duration,
                escalation_rules, tags, created_by, created_at, updated_at
            FROM alarm_rules 
            WHERE id = ?
        `,
        
        CREATE: `
            INSERT INTO alarm_rules (
                tenant_id, name, description, target_type, target_id,
                alarm_type, severity, priority, is_enabled, auto_clear,
                high_limit, low_limit, deadband, digital_trigger,
                condition_script, message_script, suppress_duration,
                escalation_rules, tags, created_by, created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        UPDATE: `
            UPDATE alarm_rules SET 
                name = ?, description = ?, target_type = ?, target_id = ?,
                alarm_type = ?, severity = ?, priority = ?, is_enabled = ?, auto_clear = ?,
                high_limit = ?, low_limit = ?, deadband = ?, digital_trigger = ?,
                condition_script = ?, message_script = ?, suppress_duration = ?,
                escalation_rules = ?, tags = ?, updated_at = ?
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
                id, tenant_id, name, description, target_type, target_id,
                alarm_type, severity, priority, is_enabled, auto_clear,
                high_limit, low_limit, deadband, digital_trigger,
                condition_script, message_script, suppress_duration,
                escalation_rules, tags, created_by, created_at, updated_at
            FROM alarm_rules 
            WHERE target_type = ? AND target_id = ?
        `,
        
        FIND_ENABLED: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id,
                alarm_type, severity, priority, is_enabled, auto_clear,
                high_limit, low_limit, deadband, digital_trigger,
                condition_script, message_script, suppress_duration,
                escalation_rules, tags, created_by, created_at, updated_at
            FROM alarm_rules 
            WHERE is_enabled = 1
        `,
        
        FIND_BY_TYPE: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id,
                alarm_type, severity, priority, is_enabled, auto_clear,
                high_limit, low_limit, deadband, digital_trigger,
                condition_script, message_script, suppress_duration,
                escalation_rules, tags, created_by, created_at, updated_at
            FROM alarm_rules 
            WHERE alarm_type = ?
        `,
        
        FIND_BY_SEVERITY: `
            SELECT 
                id, tenant_id, name, description, target_type, target_id,
                alarm_type, severity, priority, is_enabled, auto_clear,
                high_limit, low_limit, deadband, digital_trigger,
                condition_script, message_script, suppress_duration,
                escalation_rules, tags, created_by, created_at, updated_at
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
    // 🔔 AlarmOccurrence 관련 쿼리들 (C++ AlarmOccurrence 네임스페이스와 동일)
    // ========================================================================
    AlarmOccurrence: {
        // 기본 CRUD 쿼리들
        FIND_ALL: `
            SELECT 
                id, rule_id, device_id, point_id, occurrence_time, state,
                trigger_value, message, severity, priority, acknowledged_at,
                acknowledged_by, cleared_at, cleared_by, clear_comment,
                escalation_level, suppress_until, tags, created_at, updated_at
            FROM alarm_occurrences
        `,
        
        FIND_BY_ID: `
            SELECT 
                id, rule_id, device_id, point_id, occurrence_time, state,
                trigger_value, message, severity, priority, acknowledged_at,
                acknowledged_by, cleared_at, cleared_by, clear_comment,
                escalation_level, suppress_until, tags, created_at, updated_at
            FROM alarm_occurrences 
            WHERE id = ?
        `,
        
        CREATE: `
            INSERT INTO alarm_occurrences (
                rule_id, device_id, point_id, occurrence_time, state,
                trigger_value, message, severity, priority, acknowledged_at,
                acknowledged_by, cleared_at, cleared_by, clear_comment,
                escalation_level, suppress_until, tags, created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        UPDATE: `
            UPDATE alarm_occurrences SET 
                state = ?, acknowledged_at = ?, acknowledged_by = ?, 
                cleared_at = ?, cleared_by = ?, clear_comment = ?,
                escalation_level = ?, suppress_until = ?, tags = ?, updated_at = ?
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
                id, rule_id, device_id, point_id, occurrence_time, state,
                trigger_value, message, severity, priority, acknowledged_at,
                acknowledged_by, cleared_at, cleared_by, clear_comment,
                escalation_level, suppress_until, tags, created_at, updated_at
            FROM alarm_occurrences 
            WHERE state = 'active'
        `,
        
        FIND_BY_RULE: `
            SELECT 
                id, rule_id, device_id, point_id, occurrence_time, state,
                trigger_value, message, severity, priority, acknowledged_at,
                acknowledged_by, cleared_at, cleared_by, clear_comment,
                escalation_level, suppress_until, tags, created_at, updated_at
            FROM alarm_occurrences 
            WHERE rule_id = ?
        `,
        
        FIND_BY_DEVICE: `
            SELECT 
                id, rule_id, device_id, point_id, occurrence_time, state,
                trigger_value, message, severity, priority, acknowledged_at,
                acknowledged_by, cleared_at, cleared_by, clear_comment,
                escalation_level, suppress_until, tags, created_at, updated_at
            FROM alarm_occurrences 
            WHERE device_id = ?
        `,
        
        FIND_UNACKNOWLEDGED: `
            SELECT 
                id, rule_id, device_id, point_id, occurrence_time, state,
                trigger_value, message, severity, priority, acknowledged_at,
                acknowledged_by, cleared_at, cleared_by, clear_comment,
                escalation_level, suppress_until, tags, created_at, updated_at
            FROM alarm_occurrences 
            WHERE acknowledged_at IS NULL AND state = 'active'
        `,
        
        FIND_BY_SEVERITY: `
            SELECT 
                id, rule_id, device_id, point_id, occurrence_time, state,
                trigger_value, message, severity, priority, acknowledged_at,
                acknowledged_by, cleared_at, cleared_by, clear_comment,
                escalation_level, suppress_until, tags, created_at, updated_at
            FROM alarm_occurrences 
            WHERE severity = ?
        `,
        
        FIND_RECENT: `
            SELECT 
                id, rule_id, device_id, point_id, occurrence_time, state,
                trigger_value, message, severity, priority, acknowledged_at,
                acknowledged_by, cleared_at, cleared_by, clear_comment,
                escalation_level, suppress_until, tags, created_at, updated_at
            FROM alarm_occurrences 
            WHERE occurrence_time >= datetime('now', '-1 day')
            ORDER BY occurrence_time DESC
        `,
        
        // 상태 변경 쿼리들 (C++ 메서드와 동일)
        ACKNOWLEDGE: `
            UPDATE alarm_occurrences SET 
                acknowledged_at = ?, acknowledged_by = ?, updated_at = ?
            WHERE id = ?
        `,
        
        CLEAR: `
            UPDATE alarm_occurrences SET 
                state = 'cleared', cleared_at = ?, cleared_by = ?, 
                clear_comment = ?, updated_at = ?
            WHERE id = ?
        `,
        
        SUPPRESS: `
            UPDATE alarm_occurrences SET 
                suppress_until = ?, updated_at = ?
            WHERE id = ?
        `,
        
        ESCALATE: `
            UPDATE alarm_occurrences SET 
                escalation_level = escalation_level + 1, updated_at = ?
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
        
        COUNT_UNACKNOWLEDGED: `
            SELECT COUNT(*) as count 
            FROM alarm_occurrences 
            WHERE acknowledged_at IS NULL AND state = 'active'
        `,
        
        // 조건부 쿼리들
        CONDITIONS: {
            TENANT_ID: ` AND rule_id IN (SELECT id FROM alarm_rules WHERE tenant_id = ?)`,
            ACTIVE: ` AND state = 'active'`,
            ACKNOWLEDGED: ` AND acknowledged_at IS NOT NULL`,
            UNACKNOWLEDGED: ` AND acknowledged_at IS NULL`,
            ORDER_BY_TIME: ` ORDER BY occurrence_time DESC`,
            ORDER_BY_SEVERITY: ` ORDER BY CASE severity WHEN 'critical' THEN 1 WHEN 'high' THEN 2 WHEN 'medium' THEN 3 WHEN 'low' THEN 4 END ASC`,
            LIMIT: ` LIMIT ?`,
            OFFSET: ` OFFSET ?`
        }
    },
    
    // ========================================================================
    // 🔗 JOIN 쿼리들 (C++ 복합 쿼리와 동일)
    // ========================================================================
    Joins: {
        // 알람 발생 + 규칙 정보
        OCCURRENCE_WITH_RULE: `
            SELECT 
                ao.id, ao.rule_id, ao.device_id, ao.point_id, ao.occurrence_time, 
                ao.state, ao.trigger_value, ao.message, ao.severity, ao.priority,
                ao.acknowledged_at, ao.acknowledged_by, ao.cleared_at, ao.cleared_by,
                ar.name as rule_name, ar.description as rule_description,
                ar.target_type, ar.target_id, ar.alarm_type
            FROM alarm_occurrences ao
            INNER JOIN alarm_rules ar ON ao.rule_id = ar.id
        `,
        
        // 알람 발생 + 디바이스 정보 (DeviceRepository와 연동)
        OCCURRENCE_WITH_DEVICE: `
            SELECT 
                ao.id, ao.rule_id, ao.device_id, ao.point_id, ao.occurrence_time,
                ao.state, ao.trigger_value, ao.message, ao.severity, ao.priority,
                ao.acknowledged_at, ao.acknowledged_by, ao.cleared_at, ao.cleared_by,
                d.name as device_name, d.location, d.protocol_type
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
        `
    }
};

module.exports = AlarmQueries;