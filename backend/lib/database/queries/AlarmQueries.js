// =============================================================================
// backend/lib/database/queries/AlarmQueries.js
// 모든 알람 관련 SQL 쿼리를 한 곳에 모음 (C++ SQLQueries.h 패턴)
// 🔧 INSERT 오류 수정: 필수 컬럼만 사용하여 컬럼/값 개수 일치
// =============================================================================

class AlarmQueries {
    
    // =========================================================================
    // 🔥 AlarmRule 쿼리들 - INSERT 오류 수정됨
    // =========================================================================
    static AlarmRule = {
        
        // 기본 CRUD
        FIND_ALL: `
            SELECT * FROM alarm_rules 
            WHERE tenant_id = ?
            ORDER BY priority DESC, created_at DESC
        `,
        
        FIND_BY_ID: `
            SELECT * FROM alarm_rules 
            WHERE id = ? AND tenant_id = ?
        `,
        
        // 🔥 수정: 필수 컬럼 15개만 사용하여 INSERT 오류 해결
        CREATE: `
            INSERT INTO alarm_rules (
                tenant_id, name, description, target_type, target_id,
                alarm_type, severity, high_limit, low_limit, deadband,
                message_template, priority, is_enabled, auto_clear,
                notification_enabled
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        // 🔥 수정: 업데이트도 동일한 15개 컬럼 사용
        UPDATE: `
            UPDATE alarm_rules SET
                name = ?, description = ?, target_type = ?, target_id = ?,
                alarm_type = ?, severity = ?, high_limit = ?, low_limit = ?,
                deadband = ?, message_template = ?, priority = ?, is_enabled = ?,
                auto_clear = ?, notification_enabled = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        DELETE: `
            DELETE FROM alarm_rules 
            WHERE id = ? AND tenant_id = ?
        `,
        
        EXISTS: `
            SELECT 1 FROM alarm_rules 
            WHERE id = ? AND tenant_id = ? 
            LIMIT 1
        `,
        
        EXISTS_SIMPLE: `
            SELECT 1 FROM alarm_rules 
            WHERE id = ? 
            LIMIT 1
        `,
        
        // 특화 쿼리들
        FIND_BY_TARGET: `
            SELECT * FROM alarm_rules 
            WHERE target_type = ? AND target_id = ? AND tenant_id = ? AND is_enabled = 1
            ORDER BY priority DESC
        `,
        
        FIND_ENABLED: `
            SELECT * FROM alarm_rules 
            WHERE is_enabled = 1 AND tenant_id = ?
            ORDER BY priority DESC, severity DESC
        `,
        
        FIND_BY_TYPE: `
            SELECT * FROM alarm_rules 
            WHERE alarm_type = ? AND tenant_id = ?
            ORDER BY priority DESC
        `,
        
        FIND_BY_SEVERITY: `
            SELECT * FROM alarm_rules 
            WHERE severity = ? AND tenant_id = ?
            ORDER BY priority DESC
        `,
        
        // 통계 쿼리들
        COUNT_TOTAL: `
            SELECT COUNT(*) as total_rules FROM alarm_rules 
            WHERE tenant_id = ?
        `,
        
        COUNT_ENABLED: `
            SELECT COUNT(*) as enabled_rules FROM alarm_rules 
            WHERE tenant_id = ? AND is_enabled = 1
        `,
        
        STATS_BY_SEVERITY: `
            SELECT 
                severity, 
                COUNT(*) as count,
                SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count
            FROM alarm_rules 
            WHERE tenant_id = ? 
            GROUP BY severity
            ORDER BY count DESC
        `,
        
        STATS_BY_TYPE: `
            SELECT 
                alarm_type, 
                COUNT(*) as count,
                SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count
            FROM alarm_rules 
            WHERE tenant_id = ? 
            GROUP BY alarm_type
            ORDER BY count DESC
        `,
        
        STATS_SUMMARY: `
            SELECT 
                COUNT(*) as total_rules,
                SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_rules,
                COUNT(DISTINCT target_type) as target_types,
                COUNT(DISTINCT alarm_type) as alarm_types,
                COUNT(DISTINCT severity) as severity_levels
            FROM alarm_rules 
            WHERE tenant_id = ?
        `,
        
        // 검색 쿼리
        SEARCH: `
            SELECT * FROM alarm_rules 
            WHERE tenant_id = ? AND (
                name LIKE ? OR 
                description LIKE ? OR
                target_type LIKE ?
            )
            ORDER BY priority DESC, created_at DESC
        `,
        
        // 필터링 조건들 (동적으로 추가)
        CONDITIONS: {
            TARGET_TYPE: ` AND target_type = ?`,
            ALARM_TYPE: ` AND alarm_type = ?`,
            SEVERITY: ` AND severity = ?`,
            IS_ENABLED: ` AND is_enabled = ?`,
            SEARCH: ` AND (name LIKE ? OR description LIKE ?)`,
            TENANT_ID: ` AND tenant_id = ?`,
            ORDER_BY_PRIORITY: ` ORDER BY priority DESC`,
            ORDER_BY_SEVERITY: ` ORDER BY 
                CASE severity 
                    WHEN 'critical' THEN 1 
                    WHEN 'high' THEN 2 
                    WHEN 'medium' THEN 3 
                    WHEN 'low' THEN 4 
                    ELSE 5 
                END`,
            LIMIT: ` LIMIT ?`,
            OFFSET: ` OFFSET ?`
        }
    };
    
    // =========================================================================
    // 🔥 AlarmOccurrence 쿼리들 - 실제 스키마에 맞춤
    // =========================================================================
    static AlarmOccurrence = {
        
        FIND_ALL: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                ar.target_type,
                ar.target_id
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            WHERE ao.tenant_id = ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        FIND_BY_ID: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            WHERE ao.id = ? AND ao.tenant_id = ?
        `,
        
        // 🔥 수정: 실제 alarm_occurrences 스키마에 맞춘 CREATE (13개 컬럼)
        CREATE: `
            INSERT INTO alarm_occurrences (
                rule_id, tenant_id, occurrence_time, trigger_value, 
                trigger_condition, alarm_message, severity, state,
                context_data, source_name, location, device_id, point_id
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        // 🔥 수정: 실제 스키마에 맞춘 UPDATE 쿼리
        UPDATE_STATE: `
            UPDATE alarm_occurrences SET
                state = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        // 🔥 수정: 실제 스키마의 컬럼명 사용 (acknowledged_time, acknowledged_by)
        ACKNOWLEDGE: `
            UPDATE alarm_occurrences SET
                acknowledged_time = CURRENT_TIMESTAMP,
                acknowledged_by = ?,
                acknowledge_comment = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        // 🔥 수정: 실제 스키마의 컬럼명 사용 (cleared_time, cleared_value)
        CLEAR: `
            UPDATE alarm_occurrences SET
                cleared_time = CURRENT_TIMESTAMP,
                cleared_value = ?,
                clear_comment = ?,
                state = 'cleared',
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        // 🔥 수정: 실제 스키마에 맞춘 활성 알람 조회
        FIND_ACTIVE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.target_type,
                ar.priority
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            WHERE ao.tenant_id = ? AND ao.state = 'active'
            ORDER BY ar.priority DESC, ao.occurrence_time DESC
        `,
        
        // 🔥 수정: acknowledged_time IS NULL로 미확인 조회
        FIND_UNACKNOWLEDGED: `
            SELECT 
                ao.*,
                ar.name as rule_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            WHERE ao.tenant_id = ? AND ao.acknowledged_time IS NULL
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 특정 룰의 알람 이력
        FIND_BY_RULE: `
            SELECT * FROM alarm_occurrences 
            WHERE rule_id = ? AND tenant_id = ?
            ORDER BY occurrence_time DESC
        `,
        
        // 🔥 수정: 실제 스키마에 맞춘 통계 쿼리
        STATS_SUMMARY: `
            SELECT 
                COUNT(*) as total_occurrences,
                SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as active_alarms,
                SUM(CASE WHEN acknowledged_time IS NULL THEN 1 ELSE 0 END) as unacknowledged_alarms,
                SUM(CASE WHEN acknowledged_time IS NOT NULL THEN 1 ELSE 0 END) as acknowledged_alarms,
                SUM(CASE WHEN cleared_time IS NOT NULL THEN 1 ELSE 0 END) as cleared_alarms
            FROM alarm_occurrences 
            WHERE tenant_id = ?
        `,
        
        STATS_BY_SEVERITY: `
            SELECT 
                severity,
                COUNT(*) as count,
                SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as active_count
            FROM alarm_occurrences 
            WHERE tenant_id = ? 
            GROUP BY severity
            ORDER BY 
                CASE severity 
                    WHEN 'critical' THEN 1 
                    WHEN 'high' THEN 2 
                    WHEN 'medium' THEN 3 
                    WHEN 'low' THEN 4 
                    ELSE 5 
                END
        `,
        
        STATS_BY_TIME_RANGE: `
            SELECT 
                DATE(occurrence_time) as occurrence_date,
                COUNT(*) as daily_count,
                SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as active_count
            FROM alarm_occurrences 
            WHERE tenant_id = ? 
                AND occurrence_time >= ? 
                AND occurrence_time <= ?
            GROUP BY DATE(occurrence_time)
            ORDER BY occurrence_date DESC
        `,
        
        RECENT_OCCURRENCES: `
            SELECT 
                ao.*,
                ar.name as rule_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            WHERE ao.tenant_id = ? 
            ORDER BY ao.occurrence_time DESC
            LIMIT ?
        `,
        
        // 특정 기간 내 알람
        FIND_BY_DATE_RANGE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            WHERE ao.tenant_id = ? 
                AND ao.occurrence_time >= ? 
                AND ao.occurrence_time <= ?
            ORDER BY ao.occurrence_time DESC
        `
    };
    
    // =========================================================================
    // 🔥 공통 유틸리티 메서드들
    // =========================================================================
    
    /**
     * 동적 WHERE 절 생성 (AlarmRule용)
     */
    static buildAlarmRuleWhereClause(baseQuery, filters = {}) {
        let query = baseQuery;
        const params = [];
        
        if (filters.tenant_id) {
            params.push(filters.tenant_id);
        }
        
        if (filters.target_type) {
            query += this.AlarmRule.CONDITIONS.TARGET_TYPE;
            params.push(filters.target_type);
        }
        
        if (filters.alarm_type) {
            query += this.AlarmRule.CONDITIONS.ALARM_TYPE;
            params.push(filters.alarm_type);
        }
        
        if (filters.severity) {
            query += this.AlarmRule.CONDITIONS.SEVERITY;
            params.push(filters.severity);
        }
        
        if (filters.is_enabled !== undefined) {
            query += this.AlarmRule.CONDITIONS.IS_ENABLED;
            params.push(filters.is_enabled ? 1 : 0);
        }
        
        if (filters.search) {
            query += this.AlarmRule.CONDITIONS.SEARCH;
            params.push(`%${filters.search}%`, `%${filters.search}%`);
        }
        
        return { query, params };
    }
    
    /**
     * 페이징 절 추가
     */
    static addPagination(query, limit, offset) {
        if (limit) {
            query += this.AlarmRule.CONDITIONS.LIMIT;
            if (offset && offset > 0) {
                query += this.AlarmRule.CONDITIONS.OFFSET;
            }
        }
        return query;
    }

    /**
     * 정렬 절 추가
     */
    static addSorting(query, sortBy = 'priority') {
        switch (sortBy) {
            case 'priority':
                return query + this.AlarmRule.CONDITIONS.ORDER_BY_PRIORITY;
            case 'severity':
                return query + this.AlarmRule.CONDITIONS.ORDER_BY_SEVERITY;
            default:
                return query + this.AlarmRule.CONDITIONS.ORDER_BY_PRIORITY;
        }
    }

    /**
     * 🔥 CREATE 쿼리에 사용할 필수 파라미터 생성 (AlarmRule)
     * 15개 값을 정확히 제공하는 헬퍼 메서드
     */
    static buildCreateParams(data) {
        return [
            data.tenant_id,                                                 // 1
            data.name,                                                      // 2
            data.description || '',                                         // 3
            data.target_type,                                               // 4
            data.target_id,                                                 // 5
            data.alarm_type,                                                // 6
            data.severity,                                                  // 7
            data.high_limit || null,                                        // 8
            data.low_limit || null,                                         // 9
            data.deadband || 0,                                             // 10
            data.message_template || `${data.name} alarm triggered`,       // 11
            data.priority || 100,                                           // 12
            data.is_enabled !== false ? 1 : 0,                            // 13
            data.auto_clear !== false ? 1 : 0,                            // 14
            data.notification_enabled !== false ? 1 : 0                    // 15
        ];
    }

    /**
     * 🔥 CREATE 쿼리에 사용할 AlarmOccurrence 파라미터 생성
     * 13개 값을 정확히 제공하는 헬퍼 메서드
     */
    static buildCreateOccurrenceParams(data) {
        return [
            data.rule_id,                                                   // 1
            data.tenant_id,                                                 // 2
            data.occurrence_time || new Date().toISOString(),              // 3
            data.trigger_value ? JSON.stringify(data.trigger_value) : null,// 4
            data.trigger_condition || '',                                   // 5
            data.alarm_message,                                             // 6
            data.severity,                                                  // 7
            data.state || 'active',                                         // 8
            data.context_data ? JSON.stringify(data.context_data) : null,  // 9
            data.source_name || null,                                       // 10
            data.location || null,                                          // 11
            data.device_id || null,                                         // 12
            data.point_id || null                                           // 13
        ];
    }

    /**
     * AlarmOccurrence 필수 필드 검증
     */
    static validateOccurrenceRequiredFields(data) {
        const requiredFields = ['rule_id', 'tenant_id', 'alarm_message', 'severity'];
        const missingFields = [];
        
        for (const field of requiredFields) {
            if (!data[field]) {
                missingFields.push(field);
            }
        }
        
        if (missingFields.length > 0) {
            throw new Error(`필수 필드 누락: ${missingFields.join(', ')}`);
        }
        
        return true;
    }

    /**
     * 🔥 UPDATE 쿼리에 사용할 파라미터 생성
     * 16개 값 (15개 필드 + id + tenant_id)
     */
    static buildUpdateParams(data, id, tenantId) {
        return [
            data.name,                                                      // 1
            data.description || '',                                         // 2
            data.target_type,                                               // 3
            data.target_id,                                                 // 4
            data.alarm_type,                                                // 5
            data.severity,                                                  // 6
            data.high_limit || null,                                        // 7
            data.low_limit || null,                                         // 8
            data.deadband || 0,                                             // 9
            data.message_template || `${data.name} alarm triggered`,       // 10
            data.priority || 100,                                           // 11
            data.is_enabled !== false ? 1 : 0,                            // 12
            data.auto_clear !== false ? 1 : 0,                            // 13
            data.notification_enabled !== false ? 1 : 0,                   // 14
            id,                                                             // 15 (WHERE 조건)
            tenantId || data.tenant_id || 1                                 // 16 (WHERE 조건)
        ];
    }

    /**
     * 필수 필드 검증
     */
    static validateRequiredFields(data) {
        const requiredFields = ['name', 'target_type', 'target_id', 'alarm_type', 'severity'];
        const missingFields = [];
        
        for (const field of requiredFields) {
            if (!data[field]) {
                missingFields.push(field);
            }
        }
        
        if (missingFields.length > 0) {
            throw new Error(`필수 필드 누락: ${missingFields.join(', ')}`);
        }
        
        return true;
    }

    /**
     * 알람 유형별 검증
     */
    static validateAlarmTypeSpecificFields(data) {
        switch (data.alarm_type) {
            case 'analog':
                if (!data.high_limit && !data.low_limit) {
                    throw new Error('아날로그 알람은 high_limit 또는 low_limit 중 하나는 필수입니다');
                }
                break;
            case 'digital':
                if (!data.trigger_condition) {
                    throw new Error('디지털 알람은 trigger_condition이 필수입니다');
                }
                break;
            case 'script':
                if (!data.condition_script) {
                    throw new Error('스크립트 알람은 condition_script가 필수입니다');
                }
                break;
        }
        return true;
    }
}

module.exports = AlarmQueries;