// =============================================================================
// backend/lib/database/queries/AlarmQueries.js
// 모든 알람 관련 SQL 쿼리를 한 곳에 모음 - 완성된 통합 버전
// 실제 데이터베이스 스키마에 맞추어 JOIN 수정 완료
// =============================================================================

class AlarmQueries {
    
    // =========================================================================
    // 🔥 AlarmRule 쿼리들 - 실제 스키마에 맞춘 완전한 버전
    // =========================================================================
    static AlarmRule = {
        
        // 기본 CRUD - 실제 스키마 기반 JOIN (device_id, data_point_id, virtual_point_id 컬럼 사용)
        FIND_ALL: `
            SELECT 
                ar.*,
                -- 디바이스 정보
                d.name as device_name,
                d.device_type,
                d.manufacturer,
                d.model,
                
                -- 사이트 정보 (d.site_id를 통해 연결)
                s.name as site_name,
                s.location as site_location,
                s.description as site_description,
                
                -- 데이터포인트 정보
                dp.name as data_point_name,
                dp.description as data_point_description,
                dp.engineering_unit,
                dp.system_tag,
                dp.point_type,
                dp.data_type,
                
                -- 가상포인트 정보
                vp.name as virtual_point_name,
                vp.description as virtual_point_description,
                vp.calculation_formula,
                
                -- 조건 표시용 계산 필드
                CASE 
                    WHEN ar.condition_type = 'analog' THEN 
                        CASE 
                            WHEN JSON_EXTRACT(ar.condition_config, '$.high_limit') IS NOT NULL 
                                 AND JSON_EXTRACT(ar.condition_config, '$.low_limit') IS NOT NULL THEN 
                                CONCAT('범위: ', JSON_EXTRACT(ar.condition_config, '$.low_limit'), 
                                       ' ~ ', JSON_EXTRACT(ar.condition_config, '$.high_limit'), 
                                       COALESCE(dp.engineering_unit, ''))
                            WHEN JSON_EXTRACT(ar.condition_config, '$.high_limit') IS NOT NULL THEN 
                                CONCAT('상한: ', JSON_EXTRACT(ar.condition_config, '$.high_limit'), 
                                       COALESCE(dp.engineering_unit, ''))
                            WHEN JSON_EXTRACT(ar.condition_config, '$.low_limit') IS NOT NULL THEN 
                                CONCAT('하한: ', JSON_EXTRACT(ar.condition_config, '$.low_limit'), 
                                       COALESCE(dp.engineering_unit, ''))
                            ELSE ar.condition_type
                        END
                    ELSE ar.condition_type
                END as condition_display,
                
                -- 타겟 표시용 계산 필드
                CASE 
                    WHEN d.name IS NOT NULL AND dp.name IS NOT NULL THEN 
                        CONCAT(d.name, ' - ', dp.name, 
                                CASE WHEN s.location IS NOT NULL THEN CONCAT(' (', s.location, ')') ELSE '' END)
                    WHEN d.name IS NOT NULL THEN 
                        CONCAT(d.name, CASE WHEN s.location IS NOT NULL THEN CONCAT(' (', s.location, ')') ELSE '' END)
                    WHEN vp.name IS NOT NULL THEN 
                        CONCAT('가상포인트: ', vp.name)
                    ELSE CONCAT('규칙 #', ar.id)
                END as target_display
                
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.data_point_id = dp.id
            LEFT JOIN virtual_points vp ON ar.virtual_point_id = vp.id
            WHERE ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        FIND_BY_ID: `
            SELECT 
                ar.*,
                d.name as device_name,
                d.device_type,
                d.manufacturer,
                d.model,
                s.name as site_name,
                s.location as site_location,
                dp.name as data_point_name,
                dp.description as data_point_description,
                dp.engineering_unit,
                dp.system_tag,
                dp.point_type,
                dp.data_type,
                vp.name as virtual_point_name,
                vp.description as virtual_point_description,
                vp.calculation_formula,
                CASE 
                    WHEN ar.condition_type = 'analog' THEN 
                        CASE 
                            WHEN JSON_EXTRACT(ar.condition_config, '$.high_limit') IS NOT NULL 
                                 AND JSON_EXTRACT(ar.condition_config, '$.low_limit') IS NOT NULL THEN 
                                CONCAT('범위: ', JSON_EXTRACT(ar.condition_config, '$.low_limit'), 
                                       ' ~ ', JSON_EXTRACT(ar.condition_config, '$.high_limit'), 
                                       COALESCE(dp.engineering_unit, ''))
                            WHEN JSON_EXTRACT(ar.condition_config, '$.high_limit') IS NOT NULL THEN 
                                CONCAT('상한: ', JSON_EXTRACT(ar.condition_config, '$.high_limit'), 
                                       COALESCE(dp.engineering_unit, ''))
                            WHEN JSON_EXTRACT(ar.condition_config, '$.low_limit') IS NOT NULL THEN 
                                CONCAT('하한: ', JSON_EXTRACT(ar.condition_config, '$.low_limit'), 
                                       COALESCE(dp.engineering_unit, ''))
                            ELSE ar.condition_type
                        END
                    ELSE ar.condition_type
                END as condition_display,
                CASE 
                    WHEN d.name IS NOT NULL AND dp.name IS NOT NULL THEN 
                        CONCAT(d.name, ' - ', dp.name, 
                                CASE WHEN s.location IS NOT NULL THEN CONCAT(' (', s.location, ')') ELSE '' END)
                    WHEN d.name IS NOT NULL THEN 
                        CONCAT(d.name, CASE WHEN s.location IS NOT NULL THEN CONCAT(' (', s.location, ')') ELSE '' END)
                    WHEN vp.name IS NOT NULL THEN 
                        CONCAT('가상포인트: ', vp.name)
                    ELSE CONCAT('규칙 #', ar.id)
                END as target_display
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.data_point_id = dp.id
            LEFT JOIN virtual_points vp ON ar.virtual_point_id = vp.id
            WHERE ar.id = ? AND ar.tenant_id = ?
        `,
        
        // CREATE/UPDATE는 실제 스키마에 맞춤 (19개 컬럼)
        CREATE: `
            INSERT INTO alarm_rules (
                tenant_id, name, description, device_id, data_point_id,
                virtual_point_id, condition_type, condition_config, severity,
                message_template, auto_acknowledge, auto_clear, acknowledgment_required,
                escalation_time_minutes, notification_enabled, email_notification,
                sms_notification, is_enabled, created_by
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        UPDATE: `
            UPDATE alarm_rules SET
                name = ?, description = ?, device_id = ?, data_point_id = ?,
                virtual_point_id = ?, condition_type = ?, condition_config = ?,
                severity = ?, message_template = ?, auto_acknowledge = ?,
                auto_clear = ?, acknowledgment_required = ?, escalation_time_minutes = ?,
                notification_enabled = ?, email_notification = ?, sms_notification = ?,
                is_enabled = ?, updated_at = CURRENT_TIMESTAMP
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
        FIND_BY_DEVICE: `
            SELECT 
                ar.*,
                d.name as device_name,
                s.location as site_location,
                dp.name as data_point_name,
                dp.engineering_unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.data_point_id = dp.id
            WHERE ar.device_id = ? AND ar.tenant_id = ? AND ar.is_enabled = 1
            ORDER BY ar.created_at DESC
        `,
        
        FIND_ENABLED: `
            SELECT 
                ar.*,
                d.name as device_name,
                s.location as site_location,
                dp.name as data_point_name,
                dp.engineering_unit,
                CASE 
                    WHEN d.name IS NOT NULL AND dp.name IS NOT NULL THEN 
                        CONCAT(d.name, ' - ', dp.name)
                    WHEN d.name IS NOT NULL THEN d.name
                    ELSE CONCAT('규칙 #', ar.id)
                END as target_display
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.data_point_id = dp.id
            WHERE ar.is_enabled = 1 AND ar.tenant_id = ?
            ORDER BY ar.severity DESC, ar.created_at DESC
        `,
        
        FIND_BY_TYPE: `
            SELECT 
                ar.*,
                d.name as device_name,
                s.location as site_location,
                dp.name as data_point_name,
                dp.engineering_unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.data_point_id = dp.id
            WHERE ar.condition_type = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        FIND_BY_SEVERITY: `
            SELECT 
                ar.*,
                d.name as device_name,
                s.location as site_location,
                dp.name as data_point_name,
                dp.engineering_unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.data_point_id = dp.id
            WHERE ar.severity = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
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
            ORDER BY 
                CASE severity 
                    WHEN 'critical' THEN 1 
                    WHEN 'major' THEN 2 
                    WHEN 'minor' THEN 3 
                    WHEN 'warning' THEN 4 
                    WHEN 'info' THEN 5 
                    ELSE 6 
                END
        `,
        
        STATS_BY_TYPE: `
            SELECT 
                condition_type, 
                COUNT(*) as count,
                SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count
            FROM alarm_rules 
            WHERE tenant_id = ? 
            GROUP BY condition_type
            ORDER BY count DESC
        `,
        
        STATS_SUMMARY: `
            SELECT 
                COUNT(*) as total_rules,
                SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_rules,
                COUNT(DISTINCT condition_type) as condition_types,
                COUNT(DISTINCT severity) as severity_levels,
                COUNT(DISTINCT device_id) as devices_with_rules
            FROM alarm_rules 
            WHERE tenant_id = ?
        `,
        
        // 검색 쿼리 - 실제 스키마에 맞춘 JOIN
        SEARCH: `
            SELECT 
                ar.*,
                d.name as device_name,
                s.location as site_location,
                dp.name as data_point_name,
                dp.engineering_unit,
                vp.name as virtual_point_name,
                CASE 
                    WHEN d.name IS NOT NULL AND dp.name IS NOT NULL THEN 
                        CONCAT(d.name, ' - ', dp.name)
                    WHEN d.name IS NOT NULL THEN d.name
                    WHEN vp.name IS NOT NULL THEN CONCAT('가상포인트: ', vp.name)
                    ELSE CONCAT('규칙 #', ar.id)
                END as target_display,
                CASE 
                    WHEN ar.condition_type = 'analog' THEN 
                        CASE 
                            WHEN JSON_EXTRACT(ar.condition_config, '$.high_limit') IS NOT NULL 
                                 AND JSON_EXTRACT(ar.condition_config, '$.low_limit') IS NOT NULL THEN 
                                CONCAT('범위: ', JSON_EXTRACT(ar.condition_config, '$.low_limit'), 
                                       ' ~ ', JSON_EXTRACT(ar.condition_config, '$.high_limit'), 
                                       COALESCE(dp.engineering_unit, ''))
                            WHEN JSON_EXTRACT(ar.condition_config, '$.high_limit') IS NOT NULL THEN 
                                CONCAT('상한: ', JSON_EXTRACT(ar.condition_config, '$.high_limit'), 
                                       COALESCE(dp.engineering_unit, ''))
                            WHEN JSON_EXTRACT(ar.condition_config, '$.low_limit') IS NOT NULL THEN 
                                CONCAT('하한: ', JSON_EXTRACT(ar.condition_config, '$.low_limit'), 
                                       COALESCE(dp.engineering_unit, ''))
                            ELSE ar.condition_type
                        END
                    ELSE ar.condition_type
                END as condition_display
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.data_point_id = dp.id
            LEFT JOIN virtual_points vp ON ar.virtual_point_id = vp.id
            WHERE ar.tenant_id = ? AND (
                ar.name LIKE ? OR 
                ar.description LIKE ? OR
                ar.condition_type LIKE ? OR
                d.name LIKE ? OR
                dp.name LIKE ? OR
                vp.name LIKE ? OR
                s.location LIKE ?
            )
            ORDER BY ar.created_at DESC
        `,
        
        // 필터링 조건들
        CONDITIONS: {
            CONDITION_TYPE: ` AND ar.condition_type = ?`,
            SEVERITY: ` AND ar.severity = ?`,
            IS_ENABLED: ` AND ar.is_enabled = ?`,
            DEVICE_ID: ` AND ar.device_id = ?`,
            DATA_POINT_ID: ` AND ar.data_point_id = ?`,
            VIRTUAL_POINT_ID: ` AND ar.virtual_point_id = ?`,
            SEARCH: ` AND (ar.name LIKE ? OR ar.description LIKE ? OR d.name LIKE ? OR dp.name LIKE ?)`,
            TENANT_ID: ` AND ar.tenant_id = ?`,
            ORDER_BY_CREATED: ` ORDER BY ar.created_at DESC`,
            ORDER_BY_SEVERITY: ` ORDER BY 
                CASE ar.severity 
                    WHEN 'critical' THEN 1 
                    WHEN 'major' THEN 2 
                    WHEN 'minor' THEN 3 
                    WHEN 'warning' THEN 4 
                    WHEN 'info' THEN 5 
                    ELSE 6 
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
                d.name as device_name,
                s.location as site_location,
                dp.name as data_point_name,
                vp.name as virtual_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.alarm_rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ao.data_point_id = dp.id
            LEFT JOIN virtual_points vp ON ao.virtual_point_id = vp.id
            WHERE ao.tenant_id = ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        FIND_BY_ID: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                s.location as site_location,
                dp.name as data_point_name,
                vp.name as virtual_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.alarm_rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ao.data_point_id = dp.id
            LEFT JOIN virtual_points vp ON ao.virtual_point_id = vp.id
            WHERE ao.id = ? AND ao.tenant_id = ?
        `,
        
        // 실제 alarm_occurrences 스키마에 맞춘 CREATE (12개 컬럼)
        CREATE: `
            INSERT INTO alarm_occurrences (
                tenant_id, alarm_rule_id, device_id, data_point_id, virtual_point_id,
                severity, message, trigger_value, condition_details, state,
                occurrence_time, notification_sent
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        // 실제 스키마에 맞춘 UPDATE 쿼리들
        UPDATE_STATE: `
            UPDATE alarm_occurrences SET
                state = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        ACKNOWLEDGE: `
            UPDATE alarm_occurrences SET
                acknowledgment_time = CURRENT_TIMESTAMP,
                acknowledged_by = ?,
                acknowledgment_note = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        CLEAR: `
            UPDATE alarm_occurrences SET
                clear_time = CURRENT_TIMESTAMP,
                cleared_by = ?,
                resolution_note = ?,
                state = 'cleared',
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        // 활성/비활성 상태 조회
        FIND_ACTIVE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.alarm_rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.state = 'active'
            ORDER BY ao.occurrence_time DESC
        `,
        
        FIND_UNACKNOWLEDGED: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.alarm_rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.acknowledgment_time IS NULL
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 특정 룰/디바이스의 알람 이력
        FIND_BY_RULE: `
            SELECT * FROM alarm_occurrences 
            WHERE alarm_rule_id = ? AND tenant_id = ?
            ORDER BY occurrence_time DESC
        `,
        
        FIND_BY_DEVICE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.alarm_rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.device_id = ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 통계 쿼리들
        STATS_SUMMARY: `
            SELECT 
                COUNT(*) as total_occurrences,
                SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as active_alarms,
                SUM(CASE WHEN acknowledgment_time IS NULL THEN 1 ELSE 0 END) as unacknowledged_alarms,
                SUM(CASE WHEN acknowledgment_time IS NOT NULL THEN 1 ELSE 0 END) as acknowledged_alarms,
                SUM(CASE WHEN clear_time IS NOT NULL THEN 1 ELSE 0 END) as cleared_alarms
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
                    WHEN 'major' THEN 2 
                    WHEN 'minor' THEN 3 
                    WHEN 'warning' THEN 4 
                    WHEN 'info' THEN 5 
                    ELSE 6 
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
                ar.name as rule_name,
                d.name as device_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.alarm_rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? 
            ORDER BY ao.occurrence_time DESC
            LIMIT ?
        `,
        
        // 특정 기간 내 알람
        FIND_BY_DATE_RANGE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.alarm_rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? 
                AND ao.occurrence_time >= ? 
                AND ao.occurrence_time <= ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 필터링 조건들
        CONDITIONS: {
            STATE: ` AND ao.state = ?`,
            SEVERITY: ` AND ao.severity = ?`,
            RULE_ID: ` AND ao.alarm_rule_id = ?`,
            DEVICE_ID: ` AND ao.device_id = ?`,
            DATE_FROM: ` AND ao.occurrence_time >= ?`,
            DATE_TO: ` AND ao.occurrence_time <= ?`,
            ACKNOWLEDGED: ` AND ao.acknowledgment_time IS NOT NULL`,
            UNACKNOWLEDGED: ` AND ao.acknowledgment_time IS NULL`,
            CLEARED: ` AND ao.clear_time IS NOT NULL`,
            ACTIVE: ` AND ao.state = 'active'`,
            ORDER_BY_TIME: ` ORDER BY ao.occurrence_time DESC`,
            ORDER_BY_SEVERITY: ` ORDER BY 
                CASE ao.severity 
                    WHEN 'critical' THEN 1 
                    WHEN 'major' THEN 2 
                    WHEN 'minor' THEN 3 
                    WHEN 'warning' THEN 4 
                    WHEN 'info' THEN 5 
                    ELSE 6 
                END, ao.occurrence_time DESC`,
            LIMIT: ` LIMIT ?`,
            OFFSET: ` OFFSET ?`
        }
    };
    
    // =========================================================================
    // 🔥 AlarmRuleTemplate 쿼리들 - 템플릿 관리 (기존 기능 유지)
    // =========================================================================
    static AlarmTemplate = {
        
        // 기본 CRUD
        FIND_ALL: `
            SELECT * FROM alarm_rule_templates 
            WHERE tenant_id = ? AND is_active = 1
            ORDER BY category, name
        `,
        
        FIND_BY_ID: `
            SELECT * FROM alarm_rule_templates 
            WHERE id = ? AND tenant_id = ?
        `,
        
        // 템플릿 생성 (필수 컬럼만 사용)
        CREATE: `
            INSERT INTO alarm_rule_templates (
                tenant_id, name, description, category, condition_type,
                condition_template, default_config, severity, message_template,
                applicable_data_types, notification_enabled, email_notification,
                sms_notification, auto_acknowledge, auto_clear, usage_count,
                is_active, is_system_template, created_by
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        // 템플릿 수정
        UPDATE: `
            UPDATE alarm_rule_templates SET
                name = ?, description = ?, category = ?, condition_type = ?,
                condition_template = ?, default_config = ?, severity = ?,
                message_template = ?, applicable_data_types = ?, 
                notification_enabled = ?, email_notification = ?, sms_notification = ?,
                auto_acknowledge = ?, auto_clear = ?, is_active = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        DELETE: `
            UPDATE alarm_rule_templates SET 
                is_active = 0, updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        HARD_DELETE: `
            DELETE FROM alarm_rule_templates 
            WHERE id = ? AND tenant_id = ?
        `,
        
        EXISTS: `
            SELECT 1 FROM alarm_rule_templates 
            WHERE id = ? AND tenant_id = ? 
            LIMIT 1
        `,
        
        // 특화 쿼리들
        FIND_BY_CATEGORY: `
            SELECT * FROM alarm_rule_templates 
            WHERE category = ? AND tenant_id = ? AND is_active = 1
            ORDER BY name
        `,
        
        FIND_SYSTEM_TEMPLATES: `
            SELECT * FROM alarm_rule_templates 
            WHERE is_system_template = 1 AND is_active = 1
            ORDER BY category, name
        `,
        
        FIND_USER_TEMPLATES: `
            SELECT * FROM alarm_rule_templates 
            WHERE tenant_id = ? AND is_system_template = 0 AND is_active = 1
            ORDER BY category, name
        `,
        
        FIND_BY_DATA_TYPE: `
            SELECT * FROM alarm_rule_templates 
            WHERE tenant_id = ? AND is_active = 1
              AND (applicable_data_types LIKE ? OR applicable_data_types LIKE '%["*"]%')
            ORDER BY usage_count DESC, name
        `,
        
        // 사용량 업데이트
        INCREMENT_USAGE: `
            UPDATE alarm_rule_templates 
            SET usage_count = usage_count + ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
        `,
        
        // 통계 쿼리들
        COUNT_TOTAL: `
            SELECT COUNT(*) as total_templates FROM alarm_rule_templates 
            WHERE tenant_id = ? AND is_active = 1
        `,
        
        COUNT_BY_CATEGORY: `
            SELECT 
                category, 
                COUNT(*) as count,
                SUM(usage_count) as total_usage
            FROM alarm_rule_templates 
            WHERE tenant_id = ? AND is_active = 1
            GROUP BY category
            ORDER BY count DESC
        `,
        
        STATS_SUMMARY: `
            SELECT 
                COUNT(*) as total_templates,
                COUNT(DISTINCT category) as categories,
                SUM(usage_count) as total_usage,
                AVG(usage_count) as avg_usage,
                MAX(usage_count) as max_usage
            FROM alarm_rule_templates 
            WHERE tenant_id = ? AND is_active = 1
        `,
        
        MOST_USED: `
            SELECT * FROM alarm_rule_templates 
            WHERE tenant_id = ? AND is_active = 1 AND usage_count > 0
            ORDER BY usage_count DESC
            LIMIT ?
        `,
        
        // 검색 쿼리
        SEARCH: `
            SELECT * FROM alarm_rule_templates 
            WHERE tenant_id = ? AND is_active = 1 AND (
                name LIKE ? OR 
                description LIKE ? OR
                category LIKE ?
            )
            ORDER BY usage_count DESC, name
        `,
        
        // 템플릿 적용된 규칙 조회
        FIND_APPLIED_RULES: `
            SELECT ar.*, art.name as template_name
            FROM alarm_rules ar
            JOIN alarm_rule_templates art ON ar.template_id = art.id
            WHERE ar.template_id = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        APPLIED_RULES_COUNT: `
            SELECT COUNT(*) as applied_count
            FROM alarm_rules 
            WHERE template_id = ? AND tenant_id = ?
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
        
        if (filters.condition_type) {
            query += this.AlarmRule.CONDITIONS.CONDITION_TYPE;
            params.push(filters.condition_type);
        }
        
        if (filters.severity) {
            query += this.AlarmRule.CONDITIONS.SEVERITY;
            params.push(filters.severity);
        }
        
        if (filters.is_enabled !== undefined) {
            query += this.AlarmRule.CONDITIONS.IS_ENABLED;
            params.push(filters.is_enabled ? 1 : 0);
        }
        
        if (filters.device_id) {
            query += this.AlarmRule.CONDITIONS.DEVICE_ID;
            params.push(filters.device_id);
        }
        
        if (filters.data_point_id) {
            query += this.AlarmRule.CONDITIONS.DATA_POINT_ID;
            params.push(filters.data_point_id);
        }
        
        if (filters.virtual_point_id) {
            query += this.AlarmRule.CONDITIONS.VIRTUAL_POINT_ID;
            params.push(filters.virtual_point_id);
        }
        
        if (filters.search) {
            query += this.AlarmRule.CONDITIONS.SEARCH;
            params.push(`%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`);
        }
        
        return { query, params };
    }
    
    /**
     * AlarmOccurrence 필터 조건 빌더
     */
    static buildAlarmOccurrenceFilters(baseQuery, filters = {}) {
        let query = baseQuery;
        const params = [];
        
        if (filters.tenant_id) {
            params.push(filters.tenant_id);
        }
        
        if (filters.state) {
            query += this.AlarmOccurrence.CONDITIONS.STATE;
            params.push(filters.state);
        }
        
        if (filters.severity) {
            query += this.AlarmOccurrence.CONDITIONS.SEVERITY;
            params.push(filters.severity);
        }
        
        if (filters.rule_id) {
            query += this.AlarmOccurrence.CONDITIONS.RULE_ID;
            params.push(parseInt(filters.rule_id));
        }
        
        if (filters.device_id) {
            query += this.AlarmOccurrence.CONDITIONS.DEVICE_ID;
            params.push(parseInt(filters.device_id));
        }
        
        if (filters.date_from) {
            query += this.AlarmOccurrence.CONDITIONS.DATE_FROM;
            params.push(filters.date_from);
        }
        
        if (filters.date_to) {
            query += this.AlarmOccurrence.CONDITIONS.DATE_TO;
            params.push(filters.date_to);
        }
        
        if (filters.acknowledged === true) {
            query += this.AlarmOccurrence.CONDITIONS.ACKNOWLEDGED;
        } else if (filters.acknowledged === false) {
            query += this.AlarmOccurrence.CONDITIONS.UNACKNOWLEDGED;
        }
        
        return { query, params };
    }
    
    /**
     * 페이징 절 추가
     */
    static addPagination(query, limit, offset) {
        if (limit) {
            query += ` LIMIT ${parseInt(limit)}`;
            if (offset && offset > 0) {
                query += ` OFFSET ${parseInt(offset)}`;
            }
        }
        return query;
    }

    /**
     * 정렬 절 추가
     */
    static addSorting(query, sortBy = 'created_at', order = 'DESC') {
        const validSortFields = ['created_at', 'severity', 'name', 'occurrence_time'];
        const validOrders = ['ASC', 'DESC'];
        
        if (validSortFields.includes(sortBy) && validOrders.includes(order.toUpperCase())) {
            return query + ` ORDER BY ${sortBy} ${order.toUpperCase()}`;
        }
        
        return query + ` ORDER BY created_at DESC`;
    }

    /**
     * 🔥 AlarmRule CREATE 쿼리에 사용할 필수 파라미터 생성 (19개 값)
     */
    static buildCreateRuleParams(data) {
        return [
            data.tenant_id,                                                 // 1
            data.name,                                                      // 2
            data.description || '',                                         // 3
            data.device_id || null,                                         // 4
            data.data_point_id || null,                                     // 5
            data.virtual_point_id || null,                                  // 6
            data.condition_type,                                            // 7
            JSON.stringify(data.condition_config || {}),                   // 8
            data.severity || 'warning',                                     // 9
            data.message_template || `${data.name} 알람이 발생했습니다`,      // 10
            data.auto_acknowledge || 0,                                     // 11
            data.auto_clear || 0,                                          // 12
            data.acknowledgment_required !== false ? 1 : 0,               // 13
            data.escalation_time_minutes || 0,                            // 14
            data.notification_enabled !== false ? 1 : 0,                  // 15
            data.email_notification || 0,                                 // 16
            data.sms_notification || 0,                                   // 17
            data.is_enabled !== false ? 1 : 0,                           // 18
            data.created_by || null                                        // 19
        ];
    }

    /**
     * 🔥 AlarmOccurrence CREATE 쿼리에 사용할 파라미터 생성 (12개 값)
     */
    static buildCreateOccurrenceParams(data) {
        return [
            data.tenant_id,                                                 // 1
            data.alarm_rule_id,                                            // 2
            data.device_id || null,                                        // 3
            data.data_point_id || null,                                    // 4
            data.virtual_point_id || null,                                 // 5
            data.severity,                                                 // 6
            data.message,                                                  // 7
            data.trigger_value || null,                                    // 8
            data.condition_details || null,                               // 9
            data.state || 'active',                                       // 10
            data.occurrence_time || new Date().toISOString(),             // 11
            data.notification_sent || 0                                    // 12
        ];
    }

    /**
     * 🔥 AlarmRule UPDATE 쿼리에 사용할 파라미터 생성 (19개 값)
     */
    static buildUpdateRuleParams(data, id, tenantId) {
        return [
            data.name,                                                      // 1
            data.description || '',                                         // 2
            data.device_id || null,                                         // 3
            data.data_point_id || null,                                     // 4
            data.virtual_point_id || null,                                  // 5
            data.condition_type,                                            // 6
            JSON.stringify(data.condition_config || {}),                   // 7
            data.severity || 'warning',                                     // 8
            data.message_template || `${data.name} 알람이 발생했습니다`,      // 9
            data.auto_acknowledge || 0,                                     // 10
            data.auto_clear || 0,                                          // 11
            data.acknowledgment_required !== false ? 1 : 0,               // 12
            data.escalation_time_minutes || 0,                            // 13
            data.notification_enabled !== false ? 1 : 0,                  // 14
            data.email_notification || 0,                                 // 15
            data.sms_notification || 0,                                   // 16
            data.is_enabled !== false ? 1 : 0,                           // 17
            id,                                                             // 18 (WHERE 조건)
            tenantId || data.tenant_id || 1                                 // 19 (WHERE 조건)
        ];
    }

    /**
     * 템플릿 CREATE 파라미터 생성 (19개 값)
     */
    static buildCreateTemplateParams(data) {
        return [
            data.tenant_id,                                                 // 1
            data.name,                                                      // 2
            data.description || '',                                         // 3
            data.category || 'general',                                     // 4
            data.condition_type,                                            // 5
            data.condition_template,                                        // 6
            JSON.stringify(data.default_config || {}),                     // 7
            data.severity || 'warning',                                     // 8
            data.message_template || `${data.name} 알람이 발생했습니다`,      // 9
            JSON.stringify(data.applicable_data_types || ['*']),           // 10
            data.notification_enabled !== false ? 1 : 0,                  // 11
            data.email_notification || 0,                                  // 12
            data.sms_notification || 0,                                    // 13
            data.auto_acknowledge || 0,                                    // 14
            data.auto_clear !== false ? 1 : 0,                            // 15
            data.usage_count || 0,                                         // 16
            data.is_active !== false ? 1 : 0,                             // 17
            data.is_system_template || 0,                                  // 18
            data.created_by || null                                        // 19
        ];
    }

    /**
     * 템플릿 UPDATE 파라미터 생성 (17개 값)
     */
    static buildUpdateTemplateParams(data, id, tenantId) {
        return [
            data.name,                                                      // 1
            data.description || '',                                         // 2
            data.category || 'general',                                     // 3
            data.condition_type,                                            // 4
            data.condition_template,                                        // 5
            JSON.stringify(data.default_config || {}),                     // 6
            data.severity || 'warning',                                     // 7
            data.message_template || `${data.name} 알람이 발생했습니다`,      // 8
            JSON.stringify(data.applicable_data_types || ['*']),           // 9
            data.notification_enabled !== false ? 1 : 0,                  // 10
            data.email_notification || 0,                                  // 11
            data.sms_notification || 0,                                    // 12
            data.auto_acknowledge || 0,                                    // 13
            data.auto_clear !== false ? 1 : 0,                            // 14
            data.is_active !== false ? 1 : 0,                             // 15
            id,                                                             // 16 (WHERE 조건)
            tenantId || data.tenant_id || 1                                 // 17 (WHERE 조건)
        ];
    }

    /**
     * 필수 필드 검증 - AlarmRule
     */
    static validateAlarmRule(data) {
        const requiredFields = ['name', 'condition_type', 'condition_config', 'severity'];
        const missingFields = [];
        
        for (const field of requiredFields) {
            if (!data[field]) {
                missingFields.push(field);
            }
        }
        
        // 타겟 검증 (device_id, data_point_id, virtual_point_id 중 하나는 필수)
        if (!data.device_id && !data.data_point_id && !data.virtual_point_id) {
            missingFields.push('target (device_id, data_point_id, or virtual_point_id)');
        }
        
        if (missingFields.length > 0) {
            throw new Error(`필수 필드 누락: ${missingFields.join(', ')}`);
        }
        
        return true;
    }

    /**
     * 필수 필드 검증 - AlarmOccurrence
     */
    static validateAlarmOccurrence(data) {
        const requiredFields = ['tenant_id', 'alarm_rule_id', 'severity', 'message'];
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
     * 템플릿 필수 필드 검증
     */
    static validateTemplateRequiredFields(data) {
        const requiredFields = ['name', 'condition_type', 'condition_template', 'default_config'];
        const missingFields = [];
        
        for (const field of requiredFields) {
            if (!data[field]) {
                missingFields.push(field);
            }
        }
        
        if (missingFields.length > 0) {
            throw new Error(`템플릿 필수 필드 누락: ${missingFields.join(', ')}`);
        }
        
        return true;
    }

    /**
     * 알람 조건 유형별 검증
     */
    static validateConditionTypeSpecificFields(data) {
        switch (data.condition_type) {
            case 'analog':
                const config = typeof data.condition_config === 'string' 
                    ? JSON.parse(data.condition_config) 
                    : data.condition_config;
                if (!config.high_limit && !config.low_limit) {
                    throw new Error('아날로그 알람은 high_limit 또는 low_limit 중 하나는 필수입니다');
                }
                break;
            case 'digital':
                if (!data.condition_config.trigger_condition) {
                    throw new Error('디지털 알람은 trigger_condition이 필수입니다');
                }
                break;
            case 'script':
                if (!data.condition_config.condition_script) {
                    throw new Error('스크립트 알람은 condition_script가 필수입니다');
                }
                break;
        }
        return true;
    }

    /**
     * 템플릿 설정 유효성 검증
     */
    static validateTemplateConfig(data) {
        try {
            // default_config가 JSON인지 확인
            if (typeof data.default_config === 'string') {
                JSON.parse(data.default_config);
            }
            
            // applicable_data_types가 배열인지 확인
            if (data.applicable_data_types && typeof data.applicable_data_types === 'string') {
                const parsed = JSON.parse(data.applicable_data_types);
                if (!Array.isArray(parsed)) {
                    throw new Error('applicable_data_types는 배열이어야 합니다');
                }
            }
            
            return true;
        } catch (error) {
            throw new Error(`템플릿 설정 유효성 검사 실패: ${error.message}`);
        }
    }
}

module.exports = AlarmQueries;