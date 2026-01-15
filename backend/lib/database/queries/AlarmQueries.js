// =============================================================================
// backend/lib/database/queries/AlarmQueries.js
// 완전한 알람 쿼리 모음 - alarms.js의 44개 엔드포인트 완벽 지원
// 수정일: 2025-01-04 - cleared_by 버그 수정 + 모든 누락 기능 추가 완료
// =============================================================================

class AlarmQueries {

    // =========================================================================
    // AlarmRule 쿼리들 - protocols 테이블 JOIN + 모든 필터링 지원
    // =========================================================================
    static AlarmRule = {

        // 기본 CRUD - 완전한 JOIN과 계산 필드
        FIND_ALL: `
            SELECT 
                ar.*,
                -- 디바이스 정보 (target_type = 'device'일 때)
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN d.device_type END as device_type,
                CASE WHEN ar.target_type = 'device' THEN d.manufacturer END as manufacturer,
                CASE WHEN ar.target_type = 'device' THEN d.model END as model,
                
                -- 프로토콜 정보 (protocols 테이블에서 JOIN)
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,
                CASE WHEN ar.target_type = 'device' THEN p.display_name END as protocol_name,
                
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
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            LEFT JOIN devices d2 ON dp.device_id = d2.id
            LEFT JOIN protocols p2 ON d2.protocol_id = p2.id
            LEFT JOIN virtual_points vp ON ar.target_type = 'virtual_point' AND ar.target_id = vp.id
            WHERE ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,

        // 개선된 검색 쿼리 - 더 포괄적인 검색
        SEARCH: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,
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
            LEFT JOIN protocols p ON d.protocol_id = p.id
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
                p.protocol_type LIKE ? OR
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
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,
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
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            LEFT JOIN devices d2 ON dp.device_id = d2.id
            LEFT JOIN protocols p2 ON d2.protocol_id = p2.id
            LEFT JOIN virtual_points vp ON ar.target_type = 'virtual_point' AND ar.target_id = vp.id
            WHERE ar.id = ? AND ar.tenant_id = ?
        `,

        // CRUD 쿼리들
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

        // 특화 필터 쿼리들
        FIND_BY_CATEGORY: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.category = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,

        FIND_BY_TAG: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.tags LIKE ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,

        FIND_BY_TARGET: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id
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
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id
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
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id
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
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.severity = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,

        // 통계 쿼리들
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
        `,

        // 간단한 업데이트 쿼리들 (alarms.js의 PATCH 엔드포인트용)
        UPDATE_ENABLED_STATUS: `
            UPDATE alarm_rules 
            SET is_enabled = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,

        UPDATE_SETTINGS_ONLY: `
            UPDATE alarm_rules 
            SET 
                is_enabled = ?,
                notification_enabled = ?,
                auto_acknowledge = ?,
                auto_clear = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,

        UPDATE_NAME_ONLY: `
            UPDATE alarm_rules 
            SET name = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,

        UPDATE_SEVERITY_ONLY: `
            UPDATE alarm_rules 
            SET severity = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `
    };

    // =========================================================================
    // AlarmOccurrence 쿼리들 - 완전한 기능 지원 + 버그 수정
    // =========================================================================
    static AlarmOccurrence = {

        // 기본 조회 쿼리들 - device_id INTEGER 처리
        FIND_ALL: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                ar.target_type,
                ar.target_id,
                
                -- ao.device_id 기반으로 디바이스 정보 (INTEGER 처리)
                d.name as device_name,
                d.device_type,
                d.manufacturer,
                d.model,
                p.protocol_type,
                p.display_name as protocol_name,
                
                -- ao.point_id 기반으로 데이터포인트 정보 (INTEGER 처리)
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
            LEFT JOIN devices d ON d.id = ao.device_id  -- INTEGER JOIN
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id  -- INTEGER JOIN
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
                p.protocol_type,
                p.display_name as protocol_name,
                dp.name as data_point_name,
                dp.description as data_point_description,
                s.name as site_name,
                s.location as site_location,
                vp.name as virtual_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id  -- INTEGER JOIN
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id  -- INTEGER JOIN
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN virtual_points vp ON vp.id = ao.point_id
            WHERE ao.id = ? AND ao.tenant_id = ?
        `,

        // CRUD 쿼리들 - device_id, point_id INTEGER 처리
        CREATE: `
            INSERT INTO alarm_occurrences (
                rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition,
                alarm_message, severity, state, acknowledged_time, acknowledged_by,
                acknowledge_comment, cleared_time, cleared_value, clear_comment, cleared_by,
                notification_sent, notification_time, notification_count, notification_result,
                context_data, source_name, location, created_at, updated_at,
                device_id, point_id, category, tags
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,

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

        // ✅ 수정: cleared_by 파라미터 추가 (6개 파라미터)
        CLEAR: `
            UPDATE alarm_occurrences SET
                cleared_time = CURRENT_TIMESTAMP,
                cleared_value = ?,
                clear_comment = ?,
                cleared_by = ?,
                state = 'cleared',
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ? AND tenant_id = ?
        `,

        DELETE: 'DELETE FROM alarm_occurrences WHERE id = ?',

        // 활성 알람 조회 - acknowledged 필터링 지원
        FIND_ACTIVE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name,
                s.location as site_location,
                vp.name as virtual_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN virtual_points vp ON vp.id = ao.point_id
            WHERE ao.tenant_id = ? AND ao.state = 'active'
            ORDER BY ao.occurrence_time DESC
        `,

        // 미확인 알람만 조회
        FIND_UNACKNOWLEDGED: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id  -- INTEGER JOIN
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id  -- INTEGER JOIN
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.acknowledged_time IS NULL
            ORDER BY ao.occurrence_time DESC
        `,

        // ✅ 추가: 사용자가 해제한 알람 조회
        FIND_CLEARED_BY_USER: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.cleared_by = ? AND ao.tenant_id = ?
            ORDER BY ao.cleared_time DESC
        `,

        // ✅ 추가: 사용자가 확인한 알람 조회
        FIND_ACKNOWLEDGED_BY_USER: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.acknowledged_by = ? AND ao.tenant_id = ?
            ORDER BY ao.acknowledged_time DESC
        `,

        // ✅ 추가: 감사 추적 (사용자명 JOIN)
        AUDIT_TRAIL_WITH_USERS: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                u1.username as acknowledged_by_name,
                u2.username as cleared_by_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            LEFT JOIN users u1 ON ao.acknowledged_by = u1.id
            LEFT JOIN users u2 ON ao.cleared_by = u2.id
            WHERE ao.tenant_id = ?
            ORDER BY ao.updated_at DESC
        `,

        // ✅ 추가: 오늘 발생한 알람
        FIND_TODAY_ALARMS: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? 
                AND ao.occurrence_time >= ? 
                AND ao.occurrence_time < ?
            ORDER BY ao.occurrence_time DESC
        `,

        // ✅ 추가: 사용자별 처리 통계
        USER_ACTION_STATS: `
            SELECT 
                COUNT(CASE WHEN acknowledged_by = ? THEN 1 END) as acknowledged_count,
                COUNT(CASE WHEN cleared_by = ? THEN 1 END) as cleared_count,
                COUNT(CASE WHEN acknowledged_by = ? AND acknowledged_time >= datetime('now', '-7 days') THEN 1 END) as acknowledged_last_week,
                COUNT(CASE WHEN cleared_by = ? AND cleared_time >= datetime('now', '-7 days') THEN 1 END) as cleared_last_week
            FROM alarm_occurrences 
            WHERE tenant_id = ?
        `,

        // 디바이스별 조회 - device_id INTEGER 처리
        FIND_BY_DEVICE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id  -- INTEGER JOIN
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id  -- INTEGER JOIN
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.device_id = ?  -- INTEGER 비교
            ORDER BY ao.occurrence_time DESC
        `,

        FIND_ACTIVE_BY_DEVICE: `
            SELECT 
                ao.id, ao.rule_id, ao.tenant_id, ao.occurrence_time, ao.trigger_value, ao.trigger_condition,
                ao.alarm_message, ao.severity, ao.state, ao.acknowledged_time, ao.acknowledged_by,
                ao.acknowledge_comment, ao.cleared_time, ao.cleared_value, ao.clear_comment, ao.cleared_by,
                ao.notification_sent, ao.notification_time, ao.notification_count, ao.notification_result,
                ao.context_data, ao.source_name, ao.location, ao.created_at, ao.updated_at,
                ao.device_id, ao.point_id, ao.category, ao.tags,
                ar.name as rule_name, ar.description as rule_description
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            WHERE ao.tenant_id = ? 
              AND ao.device_id = ?  -- INTEGER 비교
              AND ao.state = 'active'
            ORDER BY ao.occurrence_time DESC
        `,

        // 카테고리 및 태그별 조회
        FIND_BY_CATEGORY: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id  -- INTEGER JOIN
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id  -- INTEGER JOIN
            WHERE ao.tenant_id = ? AND ao.category = ?
            ORDER BY ao.occurrence_time DESC
        `,

        FIND_BY_TAG: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id  -- INTEGER JOIN
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id  -- INTEGER JOIN
            WHERE ao.tenant_id = ? AND ao.tags LIKE ?
            ORDER BY ao.occurrence_time DESC
        `,

        // 특정 룰의 알람 이력
        FIND_BY_RULE: `
            SELECT * FROM alarm_occurrences 
            WHERE rule_id = ? AND tenant_id = ?
            ORDER BY occurrence_time DESC
        `,

        // 최근 알람
        RECENT_OCCURRENCES: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id  -- INTEGER JOIN
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id  -- INTEGER JOIN
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? 
            ORDER BY ao.occurrence_time DESC
            LIMIT ?
        `,

        // ✅ 개선: 날짜 범위 + 필터 조합 쿼리
        FIND_BY_DATE_RANGE_WITH_FILTERS: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                p.protocol_type,
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id  -- INTEGER JOIN
            LEFT JOIN protocols p ON d.protocol_id = p.id
            LEFT JOIN data_points dp ON dp.id = ao.point_id  -- INTEGER JOIN
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? 
                AND ao.occurrence_time >= ? 
                AND ao.occurrence_time <= ?
            ORDER BY ao.occurrence_time DESC
        `,

        // 통계 쿼리들
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

        // ✅ 추가: 오늘 알람 통계
        STATS_TODAY: `
            SELECT 
                COUNT(*) as today_total,
                SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as today_active,
                SUM(CASE WHEN acknowledged_time IS NULL THEN 1 ELSE 0 END) as today_unacknowledged,
                SUM(CASE WHEN severity = 'critical' THEN 1 ELSE 0 END) as today_critical,
                SUM(CASE WHEN severity = 'major' OR severity = 'high' THEN 1 ELSE 0 END) as today_major,
                SUM(CASE WHEN severity = 'minor' OR severity = 'low' THEN 1 ELSE 0 END) as today_minor,
                SUM(CASE WHEN severity = 'medium' OR severity = 'warning' THEN 1 ELSE 0 END) as today_warning
            FROM alarm_occurrences 
            WHERE tenant_id = ? 
                AND occurrence_time >= ? 
                AND occurrence_time < ?
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

        // ✅ 개선: 더 세밀한 시간 범위 통계
        STATS_BY_TIME_RANGE: `
            SELECT 
                DATE(occurrence_time) as occurrence_date,
                COUNT(*) as daily_count,
                SUM(CASE WHEN state = 'active' THEN 1 ELSE 0 END) as active_count,
                SUM(CASE WHEN severity = 'critical' THEN 1 ELSE 0 END) as critical_count,
                SUM(CASE WHEN acknowledged_time IS NULL THEN 1 ELSE 0 END) as unacknowledged_count
            FROM alarm_occurrences 
            WHERE tenant_id = ? 
                AND occurrence_time >= ? 
                AND occurrence_time <= ?
            GROUP BY DATE(occurrence_time)
            ORDER BY occurrence_date DESC
        `,

        STATS_BY_DEVICE: `
            SELECT 
                ao.device_id,
                d.name as device_name,
                COUNT(*) as total_alarms,
                SUM(CASE WHEN ao.state = 'active' THEN 1 ELSE 0 END) as active_alarms
            FROM alarm_occurrences ao
            LEFT JOIN devices d ON d.id = ao.device_id  -- INTEGER JOIN
            WHERE ao.tenant_id = ? AND ao.device_id IS NOT NULL
            GROUP BY ao.device_id, d.name
            ORDER BY total_alarms DESC
        `,

        // 개수 조회
        COUNT_ALL: `
            SELECT COUNT(*) as count 
            FROM alarm_occurrences 
            WHERE tenant_id = ?
        `,

        COUNT_ACTIVE: `
            SELECT COUNT(*) as count 
            FROM alarm_occurrences 
            WHERE tenant_id = ? AND state = 'active'
        `
    };

    // =========================================================================
    // AlarmTemplate 쿼리들 - 완전 기능 지원
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

        CREATE: `
            INSERT INTO alarm_rule_templates (
                tenant_id, name, description, category, condition_type, condition_template,
                default_config, severity, message_template, applicable_data_types,
                applicable_device_types, notification_enabled, email_notification, sms_notification,
                auto_acknowledge, auto_clear, usage_count, is_active, is_system_template, 
                created_by, created_at, tags
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        `,

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
    // 공통 유틸리티 메서드들 - 완전한 기능 + device_id INTEGER 처리
    // =========================================================================

    /**
     * ✅ 개선: AlarmOccurrence 필터 조건 빌더 - 모든 필터 지원
     */
    static buildAlarmOccurrenceFilters(baseQuery, filters = {}) {
        let query = baseQuery;
        const params = [];

        if (filters.tenant_id) {
            params.push(filters.tenant_id);
        }

        if (filters.state) {
            query += ' AND ao.state = ?';
            params.push(filters.state);
        }

        if (filters.severity) {
            query += ' AND ao.severity = ?';
            params.push(filters.severity);
        }

        if (filters.rule_id) {
            query += ' AND ao.rule_id = ?';
            params.push(parseInt(filters.rule_id));
        }

        // device_id INTEGER 검증 및 처리
        if (filters.device_id) {
            const deviceId = parseInt(filters.device_id);
            if (!isNaN(deviceId)) {
                query += ' AND ao.device_id = ?';
                params.push(deviceId);
            }
        }

        if (filters.date_from) {
            query += ' AND ao.occurrence_time >= ?';
            params.push(filters.date_from);
        }

        if (filters.date_to) {
            query += ' AND ao.occurrence_time <= ?';
            params.push(filters.date_to);
        }

        // ✅ 개선: acknowledged 필터링 로직
        if (filters.acknowledged === true || filters.acknowledged === 'true') {
            query += ' AND ao.acknowledged_time IS NOT NULL';
        } else if (filters.acknowledged === false || filters.acknowledged === 'false') {
            query += ' AND ao.acknowledged_time IS NULL';
        }

        if (filters.category) {
            query += ' AND ao.category = ?';
            params.push(filters.category);
        }

        if (filters.tag) {
            query += ' AND ao.tags LIKE ?';
            params.push(`%${filters.tag}%`);
        }

        // ✅ 추가: 사용자 필터
        if (filters.acknowledged_by) {
            query += ' AND ao.acknowledged_by = ?';
            params.push(parseInt(filters.acknowledged_by));
        }

        if (filters.cleared_by) {
            query += ' AND ao.cleared_by = ?';
            params.push(parseInt(filters.cleared_by));
        }

        return { query, params };
    }

    /**
     * ✅ 수정: AlarmOccurrence CREATE 파라미터 생성 - cleared_by 포함 (28개 파라미터)
     */
    static buildCreateOccurrenceParams(data) {
        return [
            data.rule_id,
            data.tenant_id,
            data.occurrence_time || new Date().toISOString(),
            data.trigger_value || null,
            data.trigger_condition || null,
            data.alarm_message,
            data.severity,
            data.state || 'active',
            data.acknowledged_time || null,
            data.acknowledged_by || null,
            data.acknowledge_comment || null,
            data.cleared_time || null,
            data.cleared_value || null,
            data.clear_comment || null,
            data.cleared_by || null,                    // ⭐ cleared_by 파라미터 추가!
            data.notification_sent || 0,
            data.notification_time || null,
            data.notification_count || 0,
            data.notification_result || null,
            data.context_data || null,
            data.source_name || null,
            data.location || null,
            data.created_at || new Date().toISOString(),
            data.updated_at || new Date().toISOString(),
            // device_id INTEGER 처리
            data.device_id ? parseInt(data.device_id) : null,
            // point_id INTEGER 처리
            data.point_id ? parseInt(data.point_id) : null,
            data.category || null,
            data.tags || null
        ];
    }

    /**
     * device_id INTEGER 검증 함수
     */
    static validateDeviceId(deviceId) {
        if (deviceId === null || deviceId === undefined) {
            return null;
        }

        const parsed = parseInt(deviceId);
        if (isNaN(parsed)) {
            throw new Error('device_id must be a valid integer');
        }

        return parsed;
    }

    /**
     * point_id INTEGER 검증 함수
     */
    static validatePointId(pointId) {
        if (pointId === null || pointId === undefined) {
            return null;
        }

        const parsed = parseInt(pointId);
        if (isNaN(parsed)) {
            throw new Error('point_id must be a valid integer');
        }

        return parsed;
    }

    /**
     * AlarmRule 동적 WHERE 절 생성 - 완전한 필터링 지원
     */
    static buildAlarmRuleWhereClause(baseQuery, filters = {}) {
        let query = baseQuery;
        const params = [];

        if (filters.tenantId || filters.tenant_id) {
            params.push(filters.tenantId || filters.tenant_id);
        }

        if (filters.alarm_type && filters.alarm_type !== 'all') {
            query += ' AND ar.alarm_type = ?';
            params.push(filters.alarm_type);
        }

        if (filters.severity && filters.severity !== 'all') {
            query += ' AND ar.severity = ?';
            params.push(filters.severity);
        }

        if (filters.is_enabled !== undefined) {
            query += ' AND ar.is_enabled = ?';
            params.push(filters.is_enabled ? 1 : 0);
        }

        if (filters.target_type && filters.target_type !== 'all') {
            query += ' AND ar.target_type = ?';
            params.push(filters.target_type);
        }

        if (filters.target_id || filters.device_id || filters.data_point_id || filters.virtual_point_id) {
            query += ' AND ar.target_id = ?';
            params.push(filters.target_id || filters.device_id || filters.data_point_id || filters.virtual_point_id);
        }

        if (filters.template_id) {
            query += ' AND ar.template_id = ?';
            params.push(filters.template_id);
        }

        if (filters.rule_group) {
            query += ' AND ar.rule_group = ?';
            params.push(filters.rule_group);
        }

        if (filters.category && filters.category !== 'all') {
            query += ' AND ar.category = ?';
            params.push(filters.category);
        }

        if (filters.tag && filters.tag.trim()) {
            query += ' AND ar.tags LIKE ?';
            params.push(`%${filters.tag}%`);
        }

        if (filters.protocol_type) {
            query += ' AND p.protocol_type = ?';
            params.push(filters.protocol_type);
        }

        if (filters.search) {
            query += ' AND (ar.name LIKE ? OR ar.description LIKE ? OR ar.category LIKE ? OR ar.tags LIKE ? OR p.protocol_type LIKE ?)';
            params.push(`%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`);
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
        const validSortFields = ['created_at', 'severity', 'name', 'occurrence_time', 'category', 'updated_at'];
        const validOrders = ['ASC', 'DESC'];

        if (validSortFields.includes(sortBy) && validOrders.includes(order.toUpperCase())) {
            return query + ` ORDER BY ${sortBy} ${order.toUpperCase()}`;
        }

        return query + ' ORDER BY created_at DESC';
    }

    /**
     * AlarmRule CREATE 파라미터 생성 - 41개 파라미터
     */
    static buildCreateRuleParams(data) {
        return [
            data.tenant_id,
            data.name,
            data.description || null,
            data.target_type,
            data.target_id || null,
            data.target_group || null,
            data.alarm_type,
            data.high_high_limit || null,
            data.high_limit || null,
            data.low_limit || null,
            data.low_low_limit || null,
            data.deadband || 0,
            data.rate_of_change || 0,
            data.trigger_condition || null,
            data.condition_script || null,
            data.message_script || null,
            data.message_config || null,
            data.message_template || null,
            data.severity || 'medium',
            data.priority || 100,
            data.auto_acknowledge || 0,
            data.acknowledge_timeout_min || 0,
            data.auto_clear || 0,
            data.suppression_rules || null,
            data.notification_enabled || 1,
            data.notification_delay_sec || 0,
            data.notification_repeat_interval_min || 0,
            data.notification_channels || null,
            data.notification_recipients || null,
            data.is_enabled !== undefined ? data.is_enabled ? 1 : 0 : 1,
            data.is_latched || 0,
            data.created_by || null,
            data.template_id || null,
            data.rule_group || null,
            data.created_by_template || 0,
            data.last_template_update || null,
            data.escalation_enabled || 0,
            data.escalation_max_level || 3,
            data.escalation_rules || null,
            data.category || null,
            data.tags || null
        ];
    }

    /**
     * AlarmRule UPDATE 파라미터 생성 - 42개 파라미터 (id, tenantId 포함)
     */
    static buildUpdateRuleParams(data, id, tenantId) {
        return [
            data.name,
            data.description || '',
            data.target_type || 'data_point',
            data.target_id || null,
            data.target_group || null,
            data.alarm_type || 'analog',
            data.high_high_limit || null,
            data.high_limit || null,
            data.low_limit || null,
            data.low_low_limit || null,
            data.deadband || 0,
            data.rate_of_change || 0,
            data.trigger_condition || null,
            data.condition_script || null,
            data.message_script || null,
            data.message_config || null,
            data.message_template || `${data.name} 알람이 발생했습니다`,
            data.severity || 'medium',
            data.priority || 100,
            data.auto_acknowledge || 0,
            data.acknowledge_timeout_min || 0,
            data.auto_clear || 1,
            data.suppression_rules || null,
            data.notification_enabled || 1,
            data.notification_delay_sec || 0,
            data.notification_repeat_interval_min || 0,
            data.notification_channels || null,
            data.notification_recipients || null,
            data.is_enabled !== false ? 1 : 0,
            data.is_latched || 0,
            data.template_id || null,
            data.rule_group || null,
            data.created_by_template || 0,
            data.last_template_update || null,
            data.escalation_enabled || 0,
            data.escalation_max_level || 3,
            data.escalation_rules || null,
            data.category || null,
            data.tags || null,
            id,
            tenantId
        ];
    }

    /**
     * PATCH 엔드포인트용 간단 업데이트 파라미터들
     */
    static buildEnabledStatusParams(isEnabled, id, tenantId) {
        return [
            isEnabled ? 1 : 0,
            parseInt(id),
            tenantId
        ];
    }

    static buildSettingsParams(settings, id, tenantId) {
        return [
            settings.is_enabled !== undefined ? (settings.is_enabled ? 1 : 0) : 1,
            settings.notification_enabled !== undefined ? (settings.notification_enabled ? 1 : 0) : 1,
            settings.auto_acknowledge !== undefined ? (settings.auto_acknowledge ? 1 : 0) : 0,
            settings.auto_clear !== undefined ? (settings.auto_clear ? 1 : 0) : 1,
            parseInt(id),
            tenantId
        ];
    }

    static buildNameParams(name, id, tenantId) {
        return [
            name,
            parseInt(id),
            tenantId
        ];
    }

    static buildSeverityParams(severity, id, tenantId) {
        return [
            severity,
            parseInt(id),
            tenantId
        ];
    }

    /**
     * 템플릿 CREATE 파라미터 생성 - 22개 파라미터
     */
    static buildCreateTemplateParams(data) {
        return [
            data.tenant_id,
            data.name,
            data.description || '',
            data.category || 'general',
            data.condition_type,
            data.condition_template,
            JSON.stringify(data.default_config || {}),
            data.severity || 'warning',
            data.message_template || `${data.name} 알람이 발생했습니다`,
            JSON.stringify(data.applicable_data_types || ['*']),
            JSON.stringify(data.applicable_device_types || ['*']),
            data.notification_enabled !== false ? 1 : 0,
            data.email_notification || 0,
            data.sms_notification || 0,
            data.auto_acknowledge || 0,
            data.auto_clear !== false ? 1 : 0,
            data.usage_count || 0,
            data.is_active !== false ? 1 : 0,
            data.is_system_template || 0,
            data.created_by || null,
            new Date().toISOString(),
            data.tags || null
        ];
    }

    /**
     * 템플릿 UPDATE 파라미터 생성 - 18개 파라미터 (id, tenantId 포함)
     */
    static buildUpdateTemplateParams(data, id, tenantId) {
        return [
            data.name,
            data.description || '',
            data.category || 'general',
            data.condition_type,
            data.condition_template,
            JSON.stringify(data.default_config || {}),
            data.severity || 'warning',
            data.message_template || `${data.name} 알람이 발생했습니다`,
            JSON.stringify(data.applicable_data_types || ['*']),
            JSON.stringify(data.applicable_device_types || ['*']),
            data.notification_enabled !== false ? 1 : 0,
            data.email_notification || 0,
            data.sms_notification || 0,
            data.auto_acknowledge || 0,
            data.auto_clear !== false ? 1 : 0,
            data.tags || null,
            id,
            tenantId
        ];
    }

    /**
     * ✅ 필수 필드 검증 - AlarmRule
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
     * ✅ 필수 필드 검증 - AlarmOccurrence (device_id INTEGER 검증 포함)
     */
    static validateAlarmOccurrence(data) {
        const requiredFields = ['rule_id', 'tenant_id', 'alarm_message', 'severity'];
        const missingFields = [];

        for (const field of requiredFields) {
            if (!data[field]) {
                missingFields.push(field);
            }
        }

        // device_id INTEGER 검증
        if (data.device_id !== null && data.device_id !== undefined) {
            const deviceId = parseInt(data.device_id);
            if (isNaN(deviceId)) {
                throw new Error('device_id는 유효한 정수여야 합니다');
            }
        }

        // point_id INTEGER 검증
        if (data.point_id !== null && data.point_id !== undefined) {
            const pointId = parseInt(data.point_id);
            if (isNaN(pointId)) {
                throw new Error('point_id는 유효한 정수여야 합니다');
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

    /**
     * ✅ 추가: 오늘 날짜 범위 계산 헬퍼
     */
    static getTodayDateRange() {
        const today = new Date();
        const startOfDay = new Date(today.getFullYear(), today.getMonth(), today.getDate());
        const endOfDay = new Date(today.getFullYear(), today.getMonth(), today.getDate() + 1);

        return {
            startDate: startOfDay.toISOString(),
            endDate: endOfDay.toISOString()
        };
    }

    /**
     * ✅ 추가: 특정 날짜 범위 계산 헬퍼
     */
    static getDateRange(days = 1) {
        const endDate = new Date();
        const startDate = new Date();
        startDate.setDate(startDate.getDate() - days);

        return {
            startDate: startDate.toISOString(),
            endDate: endDate.toISOString()
        };
    }
}

module.exports = AlarmQueries;