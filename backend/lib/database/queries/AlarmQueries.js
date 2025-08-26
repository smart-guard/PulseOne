// =============================================================================
// backend/lib/database/queries/AlarmQueries.js
// 프로토콜 테이블 분리에 따른 스키마 수정 버전
// 수정일: 2025-08-26 - protocols 테이블 JOIN으로 변경
// =============================================================================

class AlarmQueries {
    
    // =========================================================================
    // AlarmRule 쿼리들 - protocols 테이블 JOIN 추가
    // =========================================================================
    static AlarmRule = {
        
        // 기본 CRUD - protocols 테이블 JOIN 추가
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
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            LEFT JOIN devices d2 ON dp.device_id = d2.id
            LEFT JOIN protocols p2 ON d2.protocol_id = p2.id  -- 🔥 데이터포인트의 디바이스 프로토콜
            LEFT JOIN virtual_points vp ON ar.target_type = 'virtual_point' AND ar.target_id = vp.id
            WHERE ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        // 검색 쿼리 - protocols 테이블 JOIN 추가
        SEARCH: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,  -- 🔥 수정
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
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
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
                p.protocol_type LIKE ? OR  -- 🔥 수정: d.protocol_type → p.protocol_type
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
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,  -- 🔥 수정
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
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            LEFT JOIN devices d2 ON dp.device_id = d2.id
            LEFT JOIN protocols p2 ON d2.protocol_id = p2.id  -- 🔥 데이터포인트의 디바이스 프로토콜
            LEFT JOIN virtual_points vp ON ar.target_type = 'virtual_point' AND ar.target_id = vp.id
            WHERE ar.id = ? AND ar.tenant_id = ?
        `,
        
        // 나머지 CRUD 쿼리들은 동일 (protocols 테이블과 직접 관련 없음)
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
        
        EXISTS: `
            SELECT 1 FROM alarm_rules 
            WHERE id = ? AND tenant_id = ? 
            LIMIT 1
        `,
        
        // 카테고리별 조회 - protocols 테이블 JOIN 추가
        FIND_BY_CATEGORY: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,  -- 🔥 수정
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.category = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        // 태그별 조회 - protocols 테이블 JOIN 추가
        FIND_BY_TAG: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,  -- 🔥 수정
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.tags LIKE ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        // 특화 쿼리들 - protocols 테이블 JOIN 추가
        FIND_BY_TARGET: `
            SELECT 
                ar.*,
                CASE WHEN ar.target_type = 'device' THEN d.name END as device_name,
                CASE WHEN ar.target_type = 'device' THEN s.location END as site_location,
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,  -- 🔥 수정
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
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
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,  -- 🔥 수정
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
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
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,  -- 🔥 수정
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
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
                CASE WHEN ar.target_type = 'device' THEN p.protocol_type END as protocol_type,  -- 🔥 수정
                CASE WHEN ar.target_type = 'data_point' THEN dp.name END as data_point_name,
                CASE WHEN ar.target_type = 'data_point' THEN dp.unit END as unit
            FROM alarm_rules ar
            LEFT JOIN devices d ON ar.target_type = 'device' AND ar.target_id = d.id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN data_points dp ON ar.target_type = 'data_point' AND ar.target_id = dp.id
            WHERE ar.severity = ? AND ar.tenant_id = ?
            ORDER BY ar.created_at DESC
        `,
        
        // 통계 쿼리들 - 기존 유지 (protocols 테이블과 직접 관련 없음)
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
        
        // 간단한 업데이트 쿼리들 (기존 유지)
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
    // AlarmOccurrence 쿼리들 - protocols 테이블 JOIN 추가
    // =========================================================================
    static AlarmOccurrence = {
    
        // 기본 CRUD - protocols 테이블 JOIN 추가
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
                p.protocol_type,  -- 🔥 수정: protocols 테이블에서 가져오기
                p.display_name as protocol_name,  -- 🔥 추가
                
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
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
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
                p.protocol_type,  -- 🔥 수정: protocols 테이블에서 가져오기
                p.display_name as protocol_name,  -- 🔥 추가
                dp.name as data_point_name,
                dp.description as data_point_description,
                s.name as site_name,
                s.location as site_location,
                vp.name as virtual_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            LEFT JOIN virtual_points vp ON vp.id = ao.point_id
            WHERE ao.id = ? AND ao.tenant_id = ?
        `,
        
        // CREATE 쿼리는 변경 없음 (protocols 테이블과 직접 관련 없음)
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
        
        // 카테고리별 알람 발생 조회 - protocols 테이블 JOIN 추가
        FIND_BY_CATEGORY: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                p.protocol_type,  -- 🔥 수정
                dp.name as data_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            WHERE ao.tenant_id = ? AND ao.category = ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 태그별 알람 발생 조회 - protocols 테이블 JOIN 추가
        FIND_BY_TAG: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                p.protocol_type,  -- 🔥 수정
                dp.name as data_point_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            WHERE ao.tenant_id = ? AND ao.tags LIKE ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 활성/비활성 상태 조회 - protocols 테이블 JOIN 추가
        FIND_ACTIVE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                p.protocol_type,  -- 🔥 수정
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
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
                p.protocol_type,  -- 🔥 수정
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.acknowledged_time IS NULL
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 특정 룰/디바이스의 알람 이력 - 변경 없음
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
                p.protocol_type,  -- 🔥 수정
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? AND ao.device_id = ?
            ORDER BY ao.occurrence_time DESC
        `,
        
        // 통계 쿼리들 - 변경 없음 (protocols 테이블과 직접 관련 없음)
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
                p.protocol_type,  -- 🔥 수정
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? 
            ORDER BY ao.occurrence_time DESC
            LIMIT ?
        `,
        
        // 특정 기간 내 알람 - protocols 테이블 JOIN 추가
        FIND_BY_DATE_RANGE: `
            SELECT 
                ao.*,
                ar.name as rule_name,
                ar.severity as rule_severity,
                d.name as device_name,
                p.protocol_type,  -- 🔥 수정
                dp.name as data_point_name,
                s.location as site_location
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON d.id = ao.device_id
            LEFT JOIN protocols p ON d.protocol_id = p.id  -- 🔥 protocols 테이블 JOIN 추가
            LEFT JOIN data_points dp ON dp.id = ao.point_id
            LEFT JOIN sites s ON d.site_id = s.id
            WHERE ao.tenant_id = ? 
                AND ao.occurrence_time >= ? 
                AND ao.occurrence_time <= ?
            ORDER BY ao.occurrence_time DESC
        `
    };

    // 나머지 헬퍼 메서드들과 AlarmTemplate 쿼리들은 기존과 동일 (protocols 테이블과 직접 관련 없음)
    // ... [기존 코드들 - 변경 없음]

    /**
     * 활성화/비활성화 상태만 업데이트하는 파라미터 생성
     */
    static buildEnabledStatusParams(isEnabled, id, tenantId) {
        return [
            isEnabled ? 1 : 0,  // is_enabled
            parseInt(id),       // WHERE id = ?
            tenantId           // WHERE tenant_id = ?
        ];
    }

    /**
     * 설정만 업데이트하는 파라미터 생성
     */
    static buildSettingsParams(settings, id, tenantId) {
        return [
            settings.is_enabled !== undefined ? (settings.is_enabled ? 1 : 0) : 1,
            settings.notification_enabled !== undefined ? (settings.notification_enabled ? 1 : 0) : 1,
            settings.auto_acknowledge !== undefined ? (settings.auto_acknowledge ? 1 : 0) : 0,
            settings.auto_clear !== undefined ? (settings.auto_clear ? 1 : 0) : 1,
            parseInt(id),       // WHERE id = ?
            tenantId           // WHERE tenant_id = ?
        ];
    }

    /**
     * 이름만 업데이트하는 파라미터 생성
     */
    static buildNameParams(name, id, tenantId) {
        return [
            name,              // name = ?
            parseInt(id),      // WHERE id = ?
            tenantId          // WHERE tenant_id = ?
        ];
    }

    /**
     * 심각도만 업데이트하는 파라미터 생성
     */
    static buildSeverityParams(severity, id, tenantId) {
        return [
            severity,          // severity = ?
            parseInt(id),      // WHERE id = ?
            tenantId          // WHERE tenant_id = ?
        ];
    }
    
    // =========================================================================
    // AlarmTemplate 쿼리들 (기존과 동일 - protocols 테이블과 직접 관련 없음)
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
    // 공통 유틸리티 메서드들 (기존과 동일)
    // =========================================================================
    
    /**
     * 동적 WHERE 절 생성 (AlarmRule용) - 검색 쿼리에서 protocol_type 검색을 p.protocol_type으로 변경
     */
    static buildAlarmRuleWhereClause(baseQuery, filters = {}) {
        let query = baseQuery;
        const params = [];
        
        if (filters.tenantId || filters.tenant_id) {
            params.push(filters.tenantId || filters.tenant_id);
        }
        
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
        
        if (filters.category) {
            query += ` AND ar.category = ?`;
            params.push(filters.category);
        }
        
        if (filters.tag) {
            query += ` AND ar.tags LIKE ?`;
            params.push(`%${filters.tag}%`);
        }
        
        // 🔥 수정: protocol_type 검색 시 p.protocol_type 사용
        if (filters.protocol_type) {
            query += ` AND p.protocol_type = ?`;
            params.push(filters.protocol_type);
        }
        
        if (filters.search) {
            query += ` AND (ar.name LIKE ? OR ar.description LIKE ? OR ar.category LIKE ? OR ar.tags LIKE ? OR p.protocol_type LIKE ?)`;
            params.push(`%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`, `%${filters.search}%`);
        }
        
        return { query, params };
    }
    
    // 나머지 헬퍼 메서드들은 기존과 동일...
    
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
     * AlarmRule CREATE 파라미터 생성 (기존과 동일)
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
     * AlarmOccurrence CREATE 파라미터 생성 (기존과 동일)
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
            data.notification_sent || 0,
            data.notification_time || null,
            data.notification_count || 0,
            data.notification_result || null,
            data.context_data || null,
            data.source_name || null,
            data.location || null,
            data.created_at || new Date().toISOString(),
            data.updated_at || new Date().toISOString(),
            data.device_id || null,
            data.point_id || null,
            data.category || null,
            data.tags || null
        ];
    }

    /**
     * AlarmRule UPDATE 파라미터 생성 (기존과 동일)
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
     * 템플릿 CREATE 파라미터 생성 (기존과 동일)
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
     * 템플릿 UPDATE 파라미터 생성 (기존과 동일)
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
     * 필수 필드 검증 - AlarmRule (기존과 동일)
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
     * 필수 필드 검증 - AlarmOccurrence (기존과 동일)
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
     * 템플릿 필수 필드 검증 (기존과 동일)
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
     * 알람 조건 유형별 검증 (기존과 동일)
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
     * 템플릿 설정 유효성 검증 (기존과 동일)
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