// =============================================================================
// backend/lib/database/queries/AlarmQueries.js
// 실제 데이터베이스 스키마와 100% 일치하는 완전한 알람 쿼리 모음
// 수정일: 2025-08-21 - category, tags 컬럼 추가된 스키마 반영
// =============================================================================

class AlarmQueries {
    
    // =========================================================================
    // AlarmRule 쿼리들 - 실제 스키마 45개 컬럼 완전 호환 (category, tags 추가)
    // =========================================================================
    static AlarmRule = {
        
        // 기본 CRUD - 실제 스키마 기반 (target_type, target_id, alarm_type, category, tags 사용)
        FIND_ALL: `
            SELECT 
                ar.*,
                -- 디바이스 정보 (target_type = 'device'일 때)
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN d.device_type END as device_type,
                CASE WHEN ar.target_type = 'device' THEN d.manufacturer END as manufacturer,
                CASE WHEN ar.target_type = 'device' THEN d.model END as model,
                
                -- 사이트 정보 (디바이스를 통해)
                CASE WHEN ar.target_type = 'device' THEN s.name END as site_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'device' THEN s.description END as site_description,
                
                -- 데이터포인트 정보 (target_type = 'data_point'일 때)
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.description END as data_point_description,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit,
                CASE WHEN ar.target_type = 'data_point' THEN dp.data_type END as data_type,
                
                -- 가상포인트 정보 (target_type = 'virtual_point'일 때)
                CASE WHEN ar.target_type = 'virtual_point' THEN vp.name END as virtual_point_name,
                CASE WHEN ar.target_type = 'virtual_point' THEN vp.description END as virtual_point_description,
                CASE WHEN ar.target_type = 'virtual_point' THEN vp.formula END as calculation_formula,
                
                -- 조건 표시용 계산 필드
                CASE 
                    WHEN ar.alarm_type = 'analog' THEN 
                        CASE 
                            WHEN ar.high_limit IS NOT NULL AND ar.low_limit IS NOT NULL THEN 
                                '범위: ' || ar.low_limit || ' ~ ' || ar.high_limit || 
                                COALESCE(' ' || CASE WHEN ar.target_type = 'data_point' THEN dp.unit END, '')
                            WHEN ar.high_limit IS NOT NULL THEN 
                                '상한: ' || ar.high_limit || 
                                COALESCE(' ' || CASE WHEN ar.target_type = 'data_point' THEN dp.unit END, '')
                            WHEN ar.low_limit IS NOT NULL THEN 
                                '하한: ' || ar.low_limit || 
                                COALESCE(' ' || CASE WHEN ar.target_type = 'data_point' THEN dp.unit END, '')
                            ELSE ar.alarm_type
                        END
                    WHEN ar.alarm_type = 'digital' THEN 
                        '디지털: ' || COALESCE(ar.trigger_condition, '변화')
                    ELSE ar.alarm_type
                END as condition_display,
                
                -- 타겟 표시용 계산 필드
                CASE 
                    WHEN ar.target_type = 'device' AND d.name IS NOT NULL THEN 
                        d.name || COALESCE(' (' || s.location || ')', '')
                    WHEN ar.target_type = 'data_point' AND dp.name IS NOT NULL THEN 
                        dp.name || COALESCE(' (' || d2.name || ')', '')
                    WHEN ar.target_type = 'virtual_point' AND vp.name IS NOT NULL THEN 
                        '가상포인트: ' || vp.name
                    ELSE ar.target_type || ' #' || ar.target_id
                END as target_display
                
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            LEFT JOIN devices d2 ON dp.device_id = d2.id
            LEFT JOIN virtual_points vp ON ar.target_type = 'virtual_point' AND ar.target_id = vp.id
            WHERE ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        // 검색 쿼리 - category, tags 검색 포함
        SEARCH: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit,
                CASE WHEN ar.target_type = 'virtual_point' THEN vp.name END as virtual_point_name,
                CASE 
                    WHEN ar.target_type = 'device' AND d.name IS NOT NULL THEN 
                        d.name
                    WHEN ar.target_type = 'data_point' AND dp.name IS NOT NULL THEN 
                        dp.name
                    WHEN ar.target_type = 'virtual_point' AND vp.name IS NOT NULL THEN 
                        '가상포인트: ' || vp.name
                    ELSE ar.target_type || ' #' || ar.target_id
                END as target_display,
                CASE 
                    WHEN ar.alarm_type = 'analog' THEN 
                        CASE 
                            WHEN ar.high_limit IS NOT NULL AND ar.low_limit IS NOT NULL THEN 
                                '범위: ' || ar.low_limit || ' ~ ' || ar.high_limit || 
                                COALESCE(' ' || CASE WHEN ar.target_type = 'data_point' THEN dp.unit END, '')
                            WHEN ar.high_limit IS NOT NULL THEN 
                                '상한: ' || ar.high_limit || 
                                COALESCE(' ' || CASE WHEN ar.target_type = 'data_point' THEN dp.unit END, '')
                            WHEN ar.low_limit IS NOT NULL THEN 
                                '하한: ' || ar.low_limit || 
                                COALESCE(' ' || CASE WHEN ar.target_type = 'data_point' THEN dp.unit END, '')
                            ELSE ar.alarm_type
                        END
                    ELSE ar.alarm_type
                END as condition_display
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            LEFT JOIN virtual_points vp ON ar.target_type = 'virtual_point' AND ar.target_id = vp.id
            WHERE ar.tenant_id = ? AND (
                ar.name LIKE ? OR 
                ar.description LIKE ? OR
                ar.alarm_type LIKE ? OR
                ar.category LIKE ? OR
                ar.tags LIKE ? OR
                d.name LIKE ? OR
                dp.name LIKE ? OR
                vp.name LIKE ? OR
                s.location LIKE ?
            )
            ORDER BY ar.created_at DESC
        `,
        
        FIND_BY_ID: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN d.device_type END as device_type,
                CASE WHEN ar.target_type = 'device' THEN d.manufacturer END as manufacturer,
                CASE WHEN ar.target_type = 'device' THEN d.model END as model,
                CASE WHEN ar.target_type = 'device' THEN s.name END as site_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.description END as data_point_description,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit,
                CASE WHEN ar.target_type = 'data_point' THEN dp.data_type END as data_type,
                CASE WHEN ar.target_type = 'virtual_point' THEN vp.name END as virtual_point_name,
                CASE WHEN ar.target_type = 'virtual_point' THEN vp.description END as virtual_point_description,
                CASE WHEN ar.target_type = 'virtual_point' THEN vp.formula END as calculation_formula
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            LEFT JOIN devices d2 ON dp.device_id = d2.id
            LEFT JOIN virtual_points vp ON ar.target_type = 'virtual_point' AND ar.target_id = vp.id
            WHERE ar.id = ? AND ar.tenant_id = ?
        `,
        
        // CREATE - category, tags 컬럼 포함 (45개 컬럼에서 id 제외한 44개 값)
        CREATE: `
            INSERT INTO alarm_rules (
                tenant_id, name, description, target_type, target_id, target_group,
                alarm_type, high_high_limit, high_limit, low_limit, low_low_limit,
                deadband, rate_of_change, trigger_condition, condition_script,
                message_script, message_config, message_template, severity, priority,
                auto_acknowledge, acknowledge_timeout_min, auto_clear, suppression_rules,
                notification_enabled, notification_delay_sec, notification_repeat_interval_min,
                notification_channels, notification_recipients, is_enabled, is_latched,
                created_by, template_id, rule_group, created_by_template,
                last_template_update, escalation_enabled, escalation_max_level, escalation_rules,
                category, tags
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        // UPDATE - category, tags 컬럼 포함
        UPDATE: `
            UPDATE alarm_rules SET
                name = ?, description = ?, target_type = ?, target_id = ?, target_group = ?,
                alarm_type = ?, high_high_limit = ?, high_limit = ?, low_limit = ?, low_low_limit = ?,
                deadband = ?, rate_of_change = ?, trigger_condition = ?, condition_script = ?,
                message_script = ?, message_config = ?, message_template = ?, severity = ?, priority = ?,
                auto_acknowledge = ?, acknowledge_timeout_min = ?, auto_clear = ?, suppression_rules = ?,
                notification_enabled = ?, notification_delay_sec = ?, notification_repeat_interval_min = ?,
                notification_channels = ?, notification_recipients = ?, is_enabled = ?, is_latched = ?,
                template_id = ?, rule_group = ?, created_by_template = ?,
                last_template_update = ?, escalation_enabled = ?, escalation_max_level = ?, escalation_rules = ?,
                category = ?, tags = ?,
                updated_at = CURRENT_TIMESTAMP
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
        
        // 카테고리별 조회
        FIND_BY_CATEGORY: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.category = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        // 태그별 조회
        FIND_BY_TAG: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.tags LIKE ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        // 특화 쿼리들
        FIND_BY_TARGET: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.target_type = ? AND ar.target_id = ? AND ar.tenant_id = ? AND ar.is_enabled = 1
            ORDER BY ar.created_at DESC
        `,
        
        FIND_ENABLED: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.is_enabled = 1 AND ar.tenant_id = ?
            ORDER BY ar.severity DESC, ar.created_at DESC
        `,
        
        FIND_BY_TYPE: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.alarm_type = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        FIND_BY_SEVERITY: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.severity = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        // 통계 쿼리들 - category 포함
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
                    WHEN 'high' THEN 2 
                    WHEN 'medium' THEN 3 
                    WHEN 'low' THEN 4 
                    ELSE 5 
                END
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
        
        // 카테고리별 통계
        STATS_BY_CATEGORY: `
            SELECT 
                category, 
                COUNT(*) as count,
                SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_count
            FROM alarm_rules 
            WHERE tenant_id = ? AND category IS NOT NULL
            GROUP BY category
            ORDER BY count DESC
        `,
        
        STATS_SUMMARY: `
            SELECT 
                COUNT(*) as total_rules,
                SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as enabled_rules,
                COUNT(DISTINCT alarm_type) as alarm_types,
                COUNT(DISTINCT severity) as severity_levels,
                COUNT(DISTINCT target_type) as target_types,
                COUNT(DISTINCT category) as categories,
                COUNT(CASE WHEN tags IS NOT NULL AND tags != '[]' THEN 1 END) as rules_with_tags
            FROM alarm_rules 
            WHERE tenant_id = ?
        `
    };
    
    // =========================================================================
    // AlarmOccurrence 쿼리들 - category, tags 컬럼 포함 (28개 컬럼)
    // =========================================================================
    static AlarmOccurrence = {
    
        // 기본 CRUD - ao.device_id, ao.point_id 기반 JOIN
        FIND_ALL: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                ar.target_type,
                ar.target_id,
                
                -- ao.device_id 기반으로 디바이스 정보
                d.name as device_name,
                d.device_type,
                d.manufacturer,
                d.model,
                d.protocol_type,
                
                -- ao.point_id 기반으로 데이터포인트 정보
                dp.name as data_point_name,
                dp.description as data_point_description,
                dp.unit,
                dp.data_type,
                
                -- 사이트 정보
                s.name as site_name,
                s.location as site_location,
                
                -- 가상포인트 정보
                vp.name as virtual_point_name,
                vp.description as virtual_point_description
                
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN virtual_points vp ON vp.id = ao.point_id
            WHERE ao.tenant_id = ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        FIND_BY_ID: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                ar.target_type,
                ar.target_id,
                d.name as device_name,
                d.device_type,
                dp.name as data_point_name,
                dp.description as data_point_description,
                s.name as site_name,
                s.location as site_location,
                vp.name as virtual_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN virtual_points vp ON vp.id = ao.point_id
            WHERE ao.id = ? AND ao.tenant_id = ?
        `,
        
        // CREATE - 변경 없음
        CREATE: `
            INSERT INTO alarm_occurrences (
                rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at,
                device_id, point_id, category, tags
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        // UPDATE 쿼리들 - 변경 없음
        UPDATE_STATE: `
            UPDATE alarm_occurrences SET
                state = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        ACKNOWLEDGE: `
            UPDATE alarm_occurrences SET
                acknowledged_time = CURRENT_TIMESTAMP,
                acknowledged_by = ?,
                acknowledge_comment = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        CLEAR: `
            UPDATE alarm_occurrences SET
                cleared_time = CURRENT_TIMESTAMP,
                cleared_value = ?,
                clear_comment = ?,
                state = 'cleared',
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        // 카테고리별 알람 발생 조회
        FIND_BY_CATEGORY: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                dp.name as data_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            WHERE ao.tenant_id = ? AND ao.category = ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 태그별 알람 발생 조회
        FIND_BY_TAG: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                dp.name as data_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            WHERE ao.tenant_id = ? AND ao.tags LIKE ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 활성/비활성 상태 조회
        FIND_ACTIVE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.state = 'active'
            ORDER BY ao.occurrence_time DESC
        `,
        
        FIND_UNACKNOWLEDGED: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.acknowledged_time IS NULL
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 특정 룰/디바이스의 알람 이력
        FIND_BY_RULE: `
            SELECT * FROM alarm_occurrences 
            WHERE rule_id = ? AND tenant_id = ?
            ORDER BY occurrence_time DESC
        `,
        
        FIND_BY_DEVICE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.device_id = ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 통계 쿼리들 - 변경 없음
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
        
        // 카테고리별 발생 통계
        STATS_BY_CATEGORY: `
            SELECT 
                category,
                COUNT(*) as count,
                SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as active_count
            FROM alarm_occurrences 
            WHERE tenant_id = ? AND category IS NOT NULL
            GROUP BY category
            ORDER BY count DESC
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
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
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
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? 
                AND ao.occurrence_time >= ? 
                AND ao.occurrence_time <= ?
            ORDER BY ao.occurrence_time DESC
        `
    };
    
    // =========================================================================
    // AlarmTemplate 쿼리들 - tags 컬럼 포함 (23개 컬럼)
    // =========================================================================
    static AlarmTemplate = {
        
        FIND_ALL: `
            SELECT * FROM alarm_rule_templates 
            WHERE (tenant_id = ? OR is_system_template = 1) AND is_active = 1
            ORDER BY is_system_template DESC, name ASC
        `,
        
        FIND_BY_ID: `
            SELECT * FROM alarm_rule_templates 
            WHERE id = ? AND (tenant_id = ? OR is_system_template = 1) AND is_active = 1
        `,
        
        FIND_BY_CATEGORY: `
            SELECT * FROM alarm_rule_templates 
            WHERE category = ? AND (tenant_id = ? OR is_system_template = 1) AND is_active = 1
            ORDER BY is_system_template DESC, name ASC
        `,
        
        FIND_BY_TAG: `
            SELECT * FROM alarm_rule_templates 
            WHERE tags LIKE ? AND (tenant_id = ? OR is_system_template = 1) AND is_active = 1
            ORDER BY is_system_template DESC, name ASC
        `,
        
        FIND_SYSTEM_TEMPLATES: `
            SELECT * FROM alarm_rule_templates 
            WHERE is_system_template = 1 AND is_active = 1
            ORDER BY category ASC, name ASC
        `,
        
        FIND_BY_DATA_TYPE: `
            SELECT * FROM alarm_rule_templates 
            WHERE (tenant_id = ? OR is_system_template = 1) 
            AND is_active = 1 
            AND applicable_data_types LIKE ?
            ORDER BY is_system_template DESC, name ASC
        `,
        
        SEARCH: `
            SELECT * FROM alarm_rule_templates 
            WHERE (tenant_id = ? OR is_system_template = 1) 
            AND is_active = 1 
            AND (name LIKE ? OR description LIKE ? OR category LIKE ? OR tags LIKE ?)
            ORDER BY is_system_template DESC, name ASC
        `,
        
        MOST_USED: `
            SELECT * FROM alarm_rule_templates 
            WHERE (tenant_id = ? OR is_system_template = 1) AND is_active = 1
            ORDER BY usage_count DESC, name ASC
            LIMIT ?
        `,
        
        // CREATE - tags 컬럼 포함 (23개 컬럼에서 id 제외한 22개 값)
        CREATE: `
            INSERT INTO alarm_rule_templates (
                tenant_id, name, description, category, condition_type, condition_template,
                default_config, severity, message_template, applicable_data_types,
                applicable_device_types, notification_enabled, email_notification, sms_notification,
                auto_acknowledge, auto_clear, usage_count, is_active, is_system_template, 
                created_by, created_at, tags
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,
        
        // UPDATE - tags 컬럼 포함
        UPDATE: `
            UPDATE alarm_rule_templates SET 
                name = ?, description = ?, category = ?, condition_type = ?, condition_template = ?,
                default_config = ?, severity = ?, message_template = ?, applicable_data_types = ?,
                applicable_device_types = ?, notification_enabled = ?, email_notification = ?, 
                sms_notification = ?, auto_acknowledge = ?, auto_clear = ?, tags = ?, 
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        DELETE: `
            UPDATE alarm_rule_templates SET 
                is_active = 0, updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,
        
        INCREMENT_USAGE: `
            UPDATE alarm_rule_templates SET 
                usage_count = usage_count + ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
        `,
        
        FIND_APPLIED_RULES: `
            SELECT ar.* FROM alarm_rules ar
            WHERE ar.template_id = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        STATS_SUMMARY: `
            SELECT 
                COUNT(*) as total_templates,
                SUM(CASE WHEN is_system_template = 1 THEN 1 ELSE 0 END) as system_templates,
                SUM(CASE WHEN is_system_template = 0 THEN 1 ELSE 0 END) as custom_templates,
                COUNT(DISTINCT category) as categories,
                AVG(usage_count) as avg_usage,
                COUNT(CASE WHEN tags IS NOT NULL AND tags != '[]' THEN 1 END) as templates_with_tags
            FROM alarm_rule_templates 
            WHERE (tenant_id = ? OR is_system_template = 1) AND is_active = 1
        `,
        
        COUNT_BY_CATEGORY: `
            SELECT 
                category, 
                COUNT(*) as count,
                SUM(usage_count) as total_usage
            FROM alarm_rule_templates 
            WHERE (tenant_id = ? OR is_system_template = 1) AND is_active = 1
            GROUP BY category
            ORDER BY count DESC
        `
    };
    
    // =========================================================================
    // 공통 유틸리티 메서드들 - category, tags 필터 지원
    // =========================================================================
    
    /**
     * 동적 WHERE 절 생성 (AlarmRule용) - category, tags 필터 포함
     */
    static buildAlarmRuleWhereClause(baseQuery, filters = {}) {
        let query = baseQuery;
        const params = [];
        
        // tenant_id는 baseQuery에 이미 WHERE 절에 포함되어 있음
        if (filters.tenantId || filters.tenant_id) {
            params.push(filters.tenantId || filters.tenant_id);
        }
        
        // 추가 필터들은 AND 조건으로 추가
        if (filters.alarm_type) {
            query += ` AND ar.alarm_type = ?`;
            params.push(filters.alarm_type);
        }
        
        if (filters.severity) {
            query += ` AND ar.severity = ?`;
            params.push(filters.severity);
        }
        
        if (filters.is_enabled !== undefined) {
            query += ` AND ar.is_enabled = ?`;
            params.push(filters.is_enabled ? 1 : 0);
        }
        
        if (filters.target_type) {
            query += ` AND ar.target_type = ?`;
            params.push(filters.target_type);
        }
        
        if (filters.target_id || filters.device_id || filters.data_point_id || filters.virtual_point_id) {
            query += ` AND ar.target_id = ?`;
            params.push(filters.target_id || filters.device_id || filters.data_point_id || filters.virtual_point_id);
        }
        
        if (filters.template_id) {
            query += ` AND ar.template_id = ?`;
            params.push(filters.template_id);
        }
        
        if (filters.rule_group) {
            query += ` AND ar.rule_group = ?`;
            params.push(filters.rule_group);
        }
        
        // 새로 추가된 필터들
        if (filters.category) {
            query += ` AND ar.category = ?`;
            params.push(filters.category);
        }
        
        if (filters.tag) {
            query += ` AND ar.tags LIKE ?`;
            params.push(`%${filters.tag}%`);
        }
        
        if (filters.search) {
            query += ` AND (ar.name LIKE ? OR ar.description LIKE ? OR ar.category LIKE ? OR ar.tags LIKE ?)`;
            params.push(`%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`);
        }
        
        return { query, params };
    }
    
    /**
     * AlarmOccurrence 필터 조건 빌더 - category, tags 지원
     */
    static buildAlarmOccurrenceFilters(baseQuery, filters = {}) {
        let query = baseQuery;
        const params = [];
        
        if (filters.tenant_id) {
            params.push(filters.tenant_id);
        }
        
        if (filters.state) {
            query += ` AND ao.state = ?`;
            params.push(filters.state);
        }
        
        if (filters.severity) {
            query += ` AND ao.severity = ?`;
            params.push(filters.severity);
        }
        
        if (filters.rule_id) {
            query += ` AND ao.rule_id = ?`;
            params.push(parseInt(filters.rule_id));
        }
        
        if (filters.device_id) {
            query += ` AND ao.device_id = ?`;
            params.push(filters.device_id);
        }
        
        if (filters.date_from) {
            query += ` AND ao.occurrence_time >= ?`;
            params.push(filters.date_from);
        }
        
        if (filters.date_to) {
            query += ` AND ao.occurrence_time <= ?`;
            params.push(filters.date_to);
        }
        
        if (filters.acknowledged === true) {
            query += ` AND ao.acknowledged_time IS NOT NULL`;
        } else if (filters.acknowledged === false) {
            query += ` AND ao.acknowledged_time IS NULL`;
        }
        
        // 새로 추가된 필터들
        if (filters.category) {
            query += ` AND ao.category = ?`;
            params.push(filters.category);
        }
        
        if (filters.tag) {
            query += ` AND ao.tags LIKE ?`;
            params.push(`%${filters.tag}%`);
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
        const validSortFields = ['created_at', 'severity', 'name', 'occurrence_time', 'category'];
        const validOrders = ['ASC', 'DESC'];
        
        if (validSortFields.includes(sortBy) && validOrders.includes(order.toUpperCase())) {
            return query + ` ORDER BY ${sortBy} ${order.toUpperCase()}`;
        }
        
        return query + ` ORDER BY created_at DESC`;
    }

    /**
     * AlarmRule CREATE 파라미터 생성 (category, tags 포함 - 41개 값)
     */
    static buildCreateRuleParams(data) {
        return [
            data.tenant_id,                                         // 1
            data.name,                                              // 2
            data.description || null,                               // 3
            data.target_type,                                       // 4
            data.target_id || null,                                 // 5
            data.target_group || null,                              // 6
            data.alarm_type,                                        // 7
            data.high_high_limit || null,                           // 8
            data.high_limit || null,                                // 9
            data.low_limit || null,                                 // 10
            data.low_low_limit || null,                             // 11
            data.deadband || 0,                                     // 12
            data.rate_of_change || 0,                               // 13
            data.trigger_condition || null,                         // 14
            data.condition_script || null,                          // 15
            data.message_script || null,                            // 16
            data.message_config || null,                            // 17
            data.message_template || null,                          // 18
            data.severity || 'medium',                              // 19
            data.priority || 100,                                   // 20
            data.auto_acknowledge || 0,                             // 21
            data.acknowledge_timeout_min || 0,                      // 22
            data.auto_clear || 0,                                   // 23
            data.suppression_rules || null,                         // 24
            data.notification_enabled || 1,                         // 25
            data.notification_delay_sec || 0,                       // 26
            data.notification_repeat_interval_min || 0,             // 27
            data.notification_channels || null,                     // 28
            data.notification_recipients || null,                   // 29
            data.is_enabled !== undefined ? data.is_enabled ? 1 : 0 : 1, // 30
            data.is_latched || 0,                                   // 31
            data.created_by || null,                                // 32
            data.template_id || null,                               // 33
            data.rule_group || null,                                // 34
            data.created_by_template || 0,                          // 35
            data.last_template_update || null,                      // 36
            data.escalation_enabled || 0,                           // 37
            data.escalation_max_level || 3,                         // 38
            data.escalation_rules || null,                          // 39
            data.category || null,                                  // 40
            data.tags || null                                       // 41
        ];
    }

    /**
     * AlarmOccurrence CREATE 파라미터 생성 (category, tags 포함 - 27개 값)
     */
    static buildCreateOccurrenceParams(data) {
        return [
            data.rule_id,                                       // 1
            data.tenant_id,                                     // 2
            data.occurrence_time || new Date().toISOString(),  // 3
            data.trigger_value || null,                        // 4
            data.trigger_condition || null,                    // 5
            data.alarm_message,                                 // 6
            data.severity,                                      // 7
            data.state || 'active',                            // 8
            data.acknowledged_time || null,                    // 9
            data.acknowledged_by || null,                      // 10
            data.acknowledge_comment || null,                  // 11
            data.cleared_time || null,                         // 12
            data.cleared_value || null,                        // 13
            data.clear_comment || null,                        // 14
            data.notification_sent || 0,                       // 15
            data.notification_time || null,                    // 16
            data.notification_count || 0,                      // 17
            data.notification_result || null,                  // 18
            data.context_data || null,                         // 19
            data.source_name || null,                          // 20
            data.location || null,                             // 21
            data.created_at || new Date().toISOString(),       // 22
            data.updated_at || new Date().toISOString(),       // 23
            data.device_id || null,                            // 24
            data.point_id || null,                             // 25
            data.category || null,                             // 26
            data.tags || null                                  // 27
        ];
    }

    /**
     * AlarmRule UPDATE 파라미터 생성 (category, tags 포함 - 41개 값)
     */
    static buildUpdateRuleParams(data, id, tenantId) {
        return [
            data.name,                                          // 1
            data.description || '',                             // 2
            data.target_type || 'data_point',                  // 3
            data.target_id || null,                             // 4
            data.target_group || null,                          // 5
            data.alarm_type || 'analog',                        // 6
            data.high_high_limit || null,                       // 7
            data.high_limit || null,                            // 8
            data.low_limit || null,                             // 9
            data.low_low_limit || null,                         // 10
            data.deadband || 0,                                 // 11
            data.rate_of_change || 0,                           // 12
            data.trigger_condition || null,                     // 13
            data.condition_script || null,                      // 14
            data.message_script || null,                        // 15
            data.message_config || null,                        // 16
            data.message_template || `${data.name} 알람이 발생했습니다`,  // 17
            data.severity || 'medium',                          // 18
            data.priority || 100,                               // 19
            data.auto_acknowledge || 0,                         // 20
            data.acknowledge_timeout_min || 0,                  // 21
            data.auto_clear || 1,                               // 22
            data.suppression_rules || null,                     // 23
            data.notification_enabled || 1,                     // 24
            data.notification_delay_sec || 0,                   // 25
            data.notification_repeat_interval_min || 0,         // 26
            data.notification_channels || null,                 // 27
            data.notification_recipients || null,               // 28
            data.is_enabled !== false ? 1 : 0,                 // 29
            data.is_latched || 0,                               // 30
            data.template_id || null,                           // 31
            data.rule_group || null,                            // 32
            data.created_by_template || 0,                     // 33
            data.last_template_update || null,                 // 34
            data.escalation_enabled || 0,                      // 35
            data.escalation_max_level || 3,                    // 36
            data.escalation_rules || null,                     // 37
            data.category || null,                             // 38
            data.tags || null,                                 // 39
            id,                                                // 40 (WHERE 조건)
            tenantId                                           // 41 (WHERE 조건)
        ];
    }

    /**
     * 템플릿 CREATE 파라미터 생성 (tags 포함 - 22개 값)
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
            JSON.stringify(data.applicable_device_types || ['*']),         // 11
            data.notification_enabled !== false ? 1 : 0,                  // 12
            data.email_notification || 0,                                  // 13
            data.sms_notification || 0,                                    // 14
            data.auto_acknowledge || 0,                                    // 15
            data.auto_clear !== false ? 1 : 0,                            // 16
            data.usage_count || 0,                                         // 17
            data.is_active !== false ? 1 : 0,                             // 18
            data.is_system_template || 0,                                  // 19
            data.created_by || null,                                       // 20
            new Date().toISOString(),                                      // 21 (created_at)
            data.tags || null                                              // 22
        ];
    }

    /**
     * 템플릿 UPDATE 파라미터 생성 (tags 포함 - 18개 값)
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
            JSON.stringify(data.applicable_device_types || ['*']),         // 10
            data.notification_enabled !== false ? 1 : 0,                  // 11
            data.email_notification || 0,                                  // 12
            data.sms_notification || 0,                                    // 13
            data.auto_acknowledge || 0,                                    // 14
            data.auto_clear !== false ? 1 : 0,                            // 15
            data.tags || null,                                             // 16
            id,                                                             // 17 (WHERE 조건)
            tenantId                                                        // 18 (WHERE 조건)
        ];
    }

    /**
     * 필수 필드 검증 - AlarmRule (category, tags 추가)
     */
    static validateAlarmRule(data) {
        const requiredFields = ['name', 'target_type', 'alarm_type', 'severity'];
        const missingFields = [];
        
        for (const field of requiredFields) {
            if (!data[field]) {
                missingFields.push(field);
            }
        }
        
        // 타겟 검증 (target_type이 있을 때 target_id 필요)
        if (data.target_type && ['device', 'data_point', 'virtual_point'].includes(data.target_type) && !data.target_id) {
            missingFields.push('target_id');
        }
        
        if (missingFields.length > 0) {
            throw new Error(`필수 필드 누락: ${missingFields.join(', ')}`);
        }
        
        return true;
    }

    /**
     * 필수 필드 검증 - AlarmOccurrence (category, tags 추가)
     */
    static validateAlarmOccurrence(data) {
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
     * 템플릿 필수 필드 검증 (tags 추가)
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
        switch (data.alarm_type) {
            case 'analog':
                if (!data.high_limit && !data.low_limit && !data.high_high_limit && !data.low_low_limit) {
                    throw new Error('아날로그 알람은 임계값 중 하나는 필수입니다');
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

    /**
     * 템플릿 설정 유효성 검증 (tags 포함)
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
            
            // tags가 유효한 JSON 배열인지 확인
            if (data.tags && typeof data.tags === 'string') {
                const parsed = JSON.parse(data.tags);
                if (!Array.isArray(parsed)) {
                    throw new Error('tags는 JSON 배열이어야 합니다');
                }
            }
            
            return true;
        } catch (error) {
            throw new Error(`템플릿 설정 유효성 검사 실패: ${error.message}`);
        }
    }
}

module.exports = AlarmQueries;