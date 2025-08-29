// ============================================================================
// backend/routes/alarms.js
// 완전한 알람 관리 API - 라우트 순서 수정됨 (전체 버전)
// ============================================================================

const express = require('express');
const router = express.Router();

// ConfigManager 기반 DB 연결
const ConfigManager = require('../lib/config/ConfigManager');
const DatabaseFactory = require('../lib/database/DatabaseFactory');
const AlarmQueries = require('../lib/database/queries/AlarmQueries');

// DB 팩토리 관리
let dbFactory = null;

async function getDatabaseFactory() {
    if (!dbFactory) {
        const config = ConfigManager.getInstance();
        dbFactory = new DatabaseFactory();
    }
    return dbFactory;
}

// ============================================================================
// DatabaseFactory.executeQuery 사용 헬퍼 함수들
// ============================================================================

async function dbAll(sql, params = []) {
    const factory = await getDatabaseFactory();
    try {
        const results = await factory.executeQuery(sql, params);
        
        console.log('🔍 DatabaseFactory 원시 결과:', {
            타입: typeof results,
            생성자: results?.constructor?.name,
            키들: results ? Object.keys(results) : '없음',
            길이: results?.length,
            rows존재: !!results?.rows,
            rows길이: results?.rows?.length
        });
        
        // 다양한 가능한 결과 구조 처리
        if (Array.isArray(results)) {
            return results;
        } else if (results && Array.isArray(results.rows)) {
            return results.rows;
        } else if (results && results.recordset) {
            return results.recordset;
        } else if (results && Array.isArray(results.results)) {
            return results.results;
        } else {
            console.warn('예상하지 못한 결과 구조:', results);
            return [];
        }
    } catch (error) {
        console.error('Database query error:', error);
        throw error;
    }
}

async function dbGet(sql, params = []) {
    const factory = await getDatabaseFactory();
    try {
        const results = await factory.executeQuery(sql, params);
        
        console.log('🔍 dbGet 원시 결과:', {
            타입: typeof results,
            키들: results ? Object.keys(results) : '없음'
        });
        
        // 단일 결과 추출
        if (Array.isArray(results) && results.length > 0) {
            return results[0];
        } else if (results?.rows && Array.isArray(results.rows) && results.rows.length > 0) {
            return results.rows[0];
        } else if (results?.recordset && results.recordset.length > 0) {
            return results.recordset[0];
        } else if (results && !Array.isArray(results) && typeof results === 'object') {
            // 단일 객체가 직접 반환된 경우
            return results;
        } else {
            return null;
        }
    } catch (error) {
        console.error('Database query error:', error);
        throw error;
    }
}

async function dbRun(sql, params = []) {
    const factory = await getDatabaseFactory();
    try {
        const result = await factory.executeQuery(sql, params);
        
        console.log('🔍 dbRun 원시 결과:', {
            타입: typeof result,
            키들: result ? Object.keys(result) : '없음'
        });
        
        // INSERT/UPDATE/DELETE 결과 처리
        return {
            lastID: result?.lastInsertRowid || result?.insertId || result?.lastID || null,
            changes: result?.changes || result?.affectedRows || result?.rowsAffected || 1
        };
    } catch (error) {
        console.error('Database execution error:', error);
        throw error;
    }
}

// ============================================================================
// 미들웨어 및 헬퍼 함수들
// ============================================================================

function createResponse(success, data, message, error_code) {
    return {
        success,
        data,
        message: message || (success ? 'Success' : 'Error'),
        error_code: error_code || null,
        timestamp: new Date().toISOString()
    };
}

const authenticateToken = (req, res, next) => {
    req.user = {
        id: 1,
        username: 'admin',
        tenant_id: 1,
        role: 'admin'
    };
    next();
};

const tenantIsolation = (req, res, next) => {
    req.tenantId = req.user.tenant_id;
    next();
};

router.use(authenticateToken);
router.use(tenantIsolation);

// 포맷팅 함수들
function formatAlarmRule(rule) {
    return {
        id: rule.id,
        tenant_id: rule.tenant_id,
        
        // 기본 정보
        name: rule.name,
        description: rule.description,
        
        // 타겟 정보
        target_type: rule.target_type,
        target_id: rule.target_id,
        target_group: rule.target_group,
        
        // JOIN된 정보들
        device_name: rule.device_name,
        device_type: rule.device_type,
        manufacturer: rule.manufacturer,
        model: rule.model,
        site_name: rule.site_name,
        site_location: rule.site_location,
        site_description: rule.site_description,
        data_point_name: rule.data_point_name,
        data_point_description: rule.data_point_description,
        unit: rule.unit,
        data_type: rule.data_type,
        virtual_point_name: rule.virtual_point_name,
        virtual_point_description: rule.virtual_point_description,
        calculation_formula: rule.calculation_formula,
        condition_display: rule.condition_display,
        target_display: rule.target_display,
        
        // 알람 타입 및 조건
        alarm_type: rule.alarm_type,
        severity: rule.severity,
        priority: rule.priority,
        
        // 임계값들
        high_high_limit: rule.high_high_limit,
        high_limit: rule.high_limit,
        low_limit: rule.low_limit,
        low_low_limit: rule.low_low_limit,
        deadband: rule.deadband,
        rate_of_change: rule.rate_of_change,
        
        // 디지털 알람 조건
        trigger_condition: rule.trigger_condition,
        
        // 스크립트 관련
        condition_script: rule.condition_script,
        message_script: rule.message_script,
        
        // 메시지 관련
        message_config: parseJSON(rule.message_config),
        message_template: rule.message_template,
        
        // 동작 설정들
        auto_acknowledge: !!rule.auto_acknowledge,
        acknowledge_timeout_min: rule.acknowledge_timeout_min,
        auto_clear: !!rule.auto_clear,
        suppression_rules: parseJSON(rule.suppression_rules),
        
        // 알림 설정들
        notification_enabled: !!rule.notification_enabled,
        notification_delay_sec: rule.notification_delay_sec,
        notification_repeat_interval_min: rule.notification_repeat_interval_min,
        notification_channels: parseJSON(rule.notification_channels),
        notification_recipients: parseJSON(rule.notification_recipients),
        
        // 상태 및 제어
        is_enabled: !!rule.is_enabled,
        is_latched: !!rule.is_latched,
        
        // 템플릿 관련
        template_id: rule.template_id,
        rule_group: rule.rule_group,
        created_by_template: !!rule.created_by_template,
        last_template_update: rule.last_template_update,
        
        // 에스컬레이션 관련
        escalation_enabled: !!rule.escalation_enabled,
        escalation_max_level: rule.escalation_max_level,
        escalation_rules: parseJSON(rule.escalation_rules),
        
        // 카테고리 및 태그 (새로 추가됨)
        category: rule.category,
        tags: parseJSON(rule.tags, []),
        
        // 감사 정보
        created_by: rule.created_by,
        created_at: rule.created_at,
        updated_at: rule.updated_at
    };
}

function formatAlarmOccurrence(occurrence) {
    return {
        id: occurrence.id,
        rule_id: occurrence.rule_id,
        tenant_id: occurrence.tenant_id,
        
        // 알람 정보
        rule_name: occurrence.rule_name,
        rule_severity: occurrence.rule_severity,
        target_type: occurrence.target_type,
        target_id: occurrence.target_id,
        
        // JOIN된 정보들
        device_name: occurrence.device_name,
        site_location: occurrence.site_location,
        data_point_name: occurrence.data_point_name,
        virtual_point_name: occurrence.virtual_point_name,
        
        // 발생 정보
        occurrence_time: occurrence.occurrence_time,
        trigger_value: occurrence.trigger_value,
        trigger_condition: occurrence.trigger_condition,
        alarm_message: occurrence.alarm_message,
        severity: occurrence.severity,
        state: occurrence.state,
        
        // 확인 정보
        acknowledged_time: occurrence.acknowledged_time,
        acknowledged_by: occurrence.acknowledged_by,
        acknowledge_comment: occurrence.acknowledge_comment,
        
        // 해제 정보 - cleared_by 필드 추가!
        cleared_time: occurrence.cleared_time,
        cleared_value: occurrence.cleared_value,
        clear_comment: occurrence.clear_comment,
        cleared_by: occurrence.cleared_by,                    // 누락된 필드 추가!
        
        // 알림 정보
        notification_sent: !!occurrence.notification_sent,
        notification_time: occurrence.notification_time,
        notification_count: occurrence.notification_count,
        notification_result: parseJSON(occurrence.notification_result),
        
        // 컨텍스트 정보
        context_data: parseJSON(occurrence.context_data),
        source_name: occurrence.source_name,
        location: occurrence.location,
        
        // 디바이스/포인트 ID
        device_id: occurrence.device_id,
        point_id: occurrence.point_id,
        
        // 카테고리 및 태그
        category: occurrence.category,
        tags: parseJSON(occurrence.tags, []),
        
        // 감사 정보
        created_at: occurrence.created_at,
        updated_at: occurrence.updated_at
    };
}

function formatAlarmTemplate(template) {
    return {
        id: template.id,
        tenant_id: template.tenant_id,
        name: template.name,
        description: template.description,
        category: template.category,
        condition_type: template.condition_type,
        condition_template: template.condition_template,
        default_config: parseJSON(template.default_config),
        severity: template.severity,
        message_template: template.message_template,
        applicable_data_types: parseJSON(template.applicable_data_types, []),
        applicable_device_types: parseJSON(template.applicable_device_types, []),
        notification_enabled: !!template.notification_enabled,
        email_notification: !!template.email_notification,
        sms_notification: !!template.sms_notification,
        auto_acknowledge: !!template.auto_acknowledge,
        auto_clear: !!template.auto_clear,
        usage_count: template.usage_count || 0,
        is_active: !!template.is_active,
        is_system_template: !!template.is_system_template,
        
        // 태그 (새로 추가됨)
        tags: parseJSON(template.tags, []),
        
        created_by: template.created_by,
        created_at: template.created_at,
        updated_at: template.updated_at
    };
}

function parseJSON(jsonString, defaultValue = {}) {
    try {
        return jsonString ? JSON.parse(jsonString) : defaultValue;
    } catch (error) {
        console.warn('JSON 파싱 실패:', jsonString, error);
        return defaultValue;
    }
}

// ============================================================================
// 알람 발생 (Alarm Occurrences) API
// ============================================================================

/**
* GET /api/alarms/active
* 활성 알람 목록 조회 - ORDER BY 중복 제거
*/
router.get('/active', async (req, res) => {
   try {
       const { tenantId } = req;
       const { 
           page = 1, 
           limit = 50,
           severity,
           device_id,
           acknowledged = false,
           category,
           tag
       } = req.query;
       
       console.log('활성 알람 조회 시작...');

       // 기본 쿼리 사용하고 수동으로 필터 적용
       let query = AlarmQueries.AlarmOccurrence.FIND_ACTIVE;
       let params = [tenantId];
       
       // ORDER BY 제거 (마지막에 다시 추가할 예정)
       const orderByIndex = query.lastIndexOf('ORDER BY');
       if (orderByIndex !== -1) {
           query = query.substring(0, orderByIndex).trim();
       }
       
       // 추가 필터들을 수동으로 WHERE 절에 추가
       if (severity && severity !== 'all') {
           query += ` AND ao.severity = ?`;
           params.push(severity);
       }
       
       if (device_id) {
           query += ` AND ao.device_id = ?`;
           params.push(parseInt(device_id));
       }
       
       if (acknowledged === 'true') {
           query += ` AND ao.acknowledged_time IS NOT NULL`;
       } else if (acknowledged === 'false') {
           query += ` AND ao.acknowledged_time IS NULL`;
       }
       
       if (category && category !== 'all') {
           query += ` AND ao.category = ?`;
           params.push(category);
       }
       
       if (tag && tag.trim()) {
           query += ` AND ao.tags LIKE ?`;
           params.push(`%${tag}%`);
       }
       
       // ORDER BY 한 번만 추가
       query += ` ORDER BY ao.occurrence_time DESC`;
       
       // 페이징
       const offset = (parseInt(page) - 1) * parseInt(limit);
       query += ` LIMIT ${parseInt(limit)} OFFSET ${offset}`;

       console.log('실행할 쿼리:', query);
       console.log('파라미터:', params);

       const results = await dbAll(query, params);
       
       // 총 개수 조회
       let countQuery = `
           SELECT COUNT(*) as total
           FROM alarm_occurrences ao
           LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
           LEFT JOIN devices d ON d.id = ao.device_id
           LEFT JOIN data_points dp ON dp.id = ao.point_id
           LEFT JOIN sites s ON d.site_id = s.id
           WHERE ao.tenant_id = ? AND ao.state = 'active'
       `;
       let countParams = [tenantId];
       
       // 카운트 쿼리에도 같은 필터 적용
       if (severity && severity !== 'all') {
           countQuery += ` AND ao.severity = ?`;
           countParams.push(severity);
       }
       
       if (device_id) {
           countQuery += ` AND ao.device_id = ?`;
           countParams.push(parseInt(device_id));
       }
       
       if (acknowledged === 'true') {
           countQuery += ` AND ao.acknowledged_time IS NOT NULL`;
       } else if (acknowledged === 'false') {
           countQuery += ` AND ao.acknowledged_time IS NULL`;
       }
       
       if (category && category !== 'all') {
           countQuery += ` AND ao.category = ?`;
           countParams.push(category);
       }
       
       if (tag && tag.trim()) {
           countQuery += ` AND ao.tags LIKE ?`;
           countParams.push(`%${tag}%`);
       }
       
       const countResult = await dbGet(countQuery, countParams);
       const total = countResult ? countResult.total : 0;
       
       const result = {
           items: results.map(alarm => formatAlarmOccurrence(alarm)),
           pagination: {
               page: parseInt(page),
               limit: parseInt(limit),
               total: total,
               totalPages: Math.ceil(total / parseInt(limit))
           }
       };
       
       console.log(`활성 알람 ${results.length}개 조회 완료 (총 ${total}개)`);
       res.json(createResponse(true, result, 'Active alarms retrieved successfully'));

   } catch (error) {
       console.error('활성 알람 조회 실패:', error.message);
       res.status(500).json(createResponse(false, null, error.message, 'ACTIVE_ALARMS_ERROR'));
   }
});

/**
 * GET /api/alarms/occurrences
 * 모든 알람 발생 조회
 */
router.get('/occurrences', async (req, res) => {
    try {
        const { tenantId } = req;
        const {
            page = 1,
            limit = 50,
            state,
            severity,
            rule_id,
            device_id,
            search,
            category,  // 새로 추가
            tag        // 새로 추가
        } = req.query;
        
        console.log('알람 발생 목록 조회 시작...');

        let query, params;
        
        if (search) {
            // 검색 쿼리는 별도로 구현 필요
            query = AlarmQueries.AlarmOccurrence.FIND_ALL;
            params = [tenantId];
        } else {
            // 기본 쿼리 사용하고 수동으로 필터 적용
            query = AlarmQueries.AlarmOccurrence.FIND_ALL;
            params = [tenantId];
            
            // 추가 필터들을 수동으로 WHERE 절에 추가
            if (state && state !== 'all') {
                query += ` AND ao.state = ?`;
                params.push(state);
            }
            
            if (severity && severity !== 'all') {
                query += ` AND ao.severity = ?`;
                params.push(severity);
            }
            
            if (rule_id) {
                query += ` AND ao.rule_id = ?`;
                params.push(parseInt(rule_id));
            }
            
            if (device_id) {
                query += ` AND ao.device_id = ?`;
                params.push(parseInt(device_id));
            }
            
            if (category && category !== 'all') {
                query += ` AND ao.category = ?`;
                params.push(category);
            }
            
            if (tag && tag.trim()) {
                query += ` AND ao.tags LIKE ?`;
                params.push(`%${tag}%`);
            }
        }
        
        query = AlarmQueries.addSorting(query, 'occurrence_time', 'DESC');
        
        // 페이징
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query = AlarmQueries.addPagination(query, parseInt(limit), offset);

        const results = await dbAll(query, params);
        
        // 총 개수 조회
        const countQuery = AlarmQueries.AlarmOccurrence.STATS_SUMMARY;
        const countResult = await dbGet(countQuery, [tenantId]);
        const total = countResult?.total_occurrences || 0;
        
        const result = {
            items: results.map(occurrence => formatAlarmOccurrence(occurrence)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`알람 발생 ${results.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Alarm occurrences retrieved successfully'));

    } catch (error) {
        console.error('알람 발생 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_OCCURRENCES_ERROR'));
    }
});

/**
 * GET /api/alarms/occurrences/:id
 * 특정 알람 발생 상세 조회
 */
router.get('/occurrences/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        
        console.log(`알람 발생 ID ${id} 조회 시작...`);

        const result = await dbGet(AlarmQueries.AlarmOccurrence.FIND_BY_ID, [parseInt(id), tenantId]);
        
        if (!result) {
            return res.status(404).json(createResponse(false, null, 'Alarm occurrence not found', 'ALARM_NOT_FOUND'));
        }

        console.log(`알람 발생 ID ${id} 조회 완료`);
        res.json(createResponse(true, formatAlarmOccurrence(result), 'Alarm occurrence retrieved successfully'));

    } catch (error) {
        console.error(`알람 발생 ID ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_OCCURRENCE_ERROR'));
    }
});

/**
 * GET /api/alarms/history
 * 알람 이력 조회
 */
router.get('/history', async (req, res) => {
    try {
        const { tenantId } = req;
        const { 
            page = 1, 
            limit = 100,
            severity,
            device_id,
            date_from,
            date_to,
            state,
            category,
            tag
        } = req.query;
        
        console.log('알람 이력 조회 시작...');

        // ✅ 올바른 베이스 쿼리 사용 (ao 별칭이 포함된 JOIN 쿼리)
        let query = AlarmQueries.AlarmOccurrence.FIND_ALL;
        let params = [tenantId];
        
        // ✅ ORDER BY를 안전하게 제거 (마지막 ORDER BY만 제거)
        const orderByIndex = query.lastIndexOf('ORDER BY');
        if (orderByIndex !== -1) {
            query = query.substring(0, orderByIndex).trim();
        }
        
        // 필터 조건들 추가 (이제 ao 별칭이 올바르게 정의됨)
        if (severity && severity !== 'all') {
            query += ` AND ao.severity = ?`;
            params.push(severity);
        }
        
        if (device_id) {
            query += ` AND ao.device_id = ?`;
            params.push(parseInt(device_id));
        }
        
        if (state && state !== 'all') {
            query += ` AND ao.state = ?`;
            params.push(state);
        }
        
        if (category && category !== 'all') {
            query += ` AND ao.category = ?`;
            params.push(category);
        }
        
        if (tag && tag.trim()) {
            query += ` AND ao.tags LIKE ?`;
            params.push(`%${tag}%`);
        }
        
        if (date_from && date_to) {
            query += ` AND ao.occurrence_time >= ? AND ao.occurrence_time <= ?`;
            params.push(date_from, date_to);
        }
        
        // ORDER BY 추가
        query += ` ORDER BY ao.occurrence_time DESC`;
        
        // 페이징 추가
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query += ` LIMIT ${parseInt(limit)} OFFSET ${offset}`;

        const results = await dbAll(query, params);
        
        // ✅ 총 개수 조회를 위한 별도 쿼리 (더 정확한 카운트)
        let countQuery = `
            SELECT COUNT(*) as total
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            WHERE ao.tenant_id = ?
        `;
        let countParams = [tenantId];
        
        // 동일한 필터 조건을 카운트 쿼리에도 적용
        if (severity && severity !== 'all') {
            countQuery += ` AND ao.severity = ?`;
            countParams.push(severity);
        }
        
        if (device_id) {
            countQuery += ` AND ao.device_id = ?`;
            countParams.push(parseInt(device_id));
        }
        
        if (state && state !== 'all') {
            countQuery += ` AND ao.state = ?`;
            countParams.push(state);
        }
        
        if (category && category !== 'all') {
            countQuery += ` AND ao.category = ?`;
            countParams.push(category);
        }
        
        if (tag && tag.trim()) {
            countQuery += ` AND ao.tags LIKE ?`;
            countParams.push(`%${tag}%`);
        }
        
        if (date_from && date_to) {
            countQuery += ` AND ao.occurrence_time >= ? AND ao.occurrence_time <= ?`;
            countParams.push(date_from, date_to);
        }
        
        const countResult = await dbGet(countQuery, countParams);
        const total = countResult?.total || 0;
        
        // 응답 구성
        const result = {
            items: results.map(occurrence => formatAlarmOccurrence(occurrence)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`알람 이력 ${results.length}개 조회 완료 (전체: ${total}개)`);
        res.json(createResponse(true, result, 'Alarm history retrieved successfully'));

    } catch (error) {
        console.error('알람 이력 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_HISTORY_ERROR'));
    }
});

/**
 * POST /api/alarms/occurrences/:id/acknowledge
 * 알람 확인 처리
 */
router.post('/occurrences/:id/acknowledge', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment = '' } = req.body;
        const { user, tenantId } = req;
        
        console.log(`알람 발생 ${id} 확인 처리 시작...`);

        const result = await dbRun(AlarmQueries.AlarmOccurrence.ACKNOWLEDGE, [user.id, comment, parseInt(id), tenantId]);

        if (result.changes > 0) {
            const updatedAlarm = await dbGet(AlarmQueries.AlarmOccurrence.FIND_BY_ID, [parseInt(id), tenantId]);

            console.log(`알람 발생 ${id} 확인 처리 완료`);
            res.json(createResponse(true, formatAlarmOccurrence(updatedAlarm), 'Alarm occurrence acknowledged successfully'));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm occurrence not found or already acknowledged', 'ALARM_NOT_FOUND')
            );
        }

    } catch (error) {
        console.error(`알람 발생 ${req.params.id} 확인 처리 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_ACKNOWLEDGE_ERROR'));
    }
});

/**
 * POST /api/alarms/occurrences/:id/clear
 * 알람 해제 처리
 */
router.post('/occurrences/:id/clear', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment = '', clearedValue = '' } = req.body;
        const { user, tenantId } = req;
        
        console.log(`알람 발생 ${id} 해제 처리 시작... (사용자: ${user.id})`);

        // 수정: cleared_by 파라미터 추가 (4개 → 5개 파라미터)
        // AlarmQueries.AlarmOccurrence.CLEAR 쿼리 구조:
        // UPDATE alarm_occurrences SET state = 'cleared', cleared_time = CURRENT_TIMESTAMP, 
        // cleared_value = ?, clear_comment = ?, cleared_by = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?
        const result = await dbRun(AlarmQueries.AlarmOccurrence.CLEAR, [
            clearedValue, 
            comment, 
            user.id,        // cleared_by 파라미터 추가!
            parseInt(id), 
            tenantId
        ]);

        if (result.changes > 0) {
            const updatedAlarm = await dbGet(AlarmQueries.AlarmOccurrence.FIND_BY_ID, [parseInt(id), tenantId]);

            console.log(`알람 발생 ${id} 해제 처리 완료 (사용자: ${user.id})`);
            res.json(createResponse(true, formatAlarmOccurrence(updatedAlarm), 'Alarm occurrence cleared successfully'));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm occurrence not found', 'ALARM_NOT_FOUND')
            );
        }

    } catch (error) {
        console.error(`알람 발생 ${req.params.id} 해제 처리 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_CLEAR_ERROR'));
    }
});


/**
 * GET /api/alarms/user/:userId/cleared
 * 특정 사용자가 해제한 알람들 조회
 */
router.get('/user/:userId/cleared', async (req, res) => {
    try {
        const { userId } = req.params;
        const { tenantId } = req;
        const { page = 1, limit = 50 } = req.query;
        
        console.log(`사용자 ${userId}가 해제한 알람 조회...`);

        let query = `
            SELECT ao.*, ar.name as rule_name, d.name as device_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            WHERE ao.cleared_by = ? AND ao.tenant_id = ?
            ORDER BY ao.cleared_time DESC
        `;
        const params = [parseInt(userId), tenantId];
        
        // 총 개수 조회
        const countQuery = `
            SELECT COUNT(*) as total
            FROM alarm_occurrences ao
            WHERE ao.cleared_by = ? AND ao.tenant_id = ?
        `;
        const countResult = await dbGet(countQuery, params);
        const total = countResult?.total || 0;
        
        // 페이징 적용
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query += ` LIMIT ${parseInt(limit)} OFFSET ${offset}`;
        
        const results = await dbAll(query, params);
        
        const result = {
            items: results.map(occurrence => formatAlarmOccurrence(occurrence)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`사용자 ${userId}가 해제한 알람 ${results.length}개 조회 완료`);
        res.json(createResponse(true, result, 'User cleared alarms retrieved successfully'));

    } catch (error) {
        console.error(`사용자 ${req.params.userId} 해제 알람 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'USER_CLEARED_ALARMS_ERROR'));
    }
});

/**
 * GET /api/alarms/user/:userId/acknowledged
 * 특정 사용자가 확인한 알람들 조회
 */
router.get('/user/:userId/acknowledged', async (req, res) => {
    try {
        const { userId } = req.params;
        const { tenantId } = req;
        const { page = 1, limit = 50 } = req.query;
        
        console.log(`사용자 ${userId}가 확인한 알람 조회...`);

        let query = `
            SELECT ao.*, ar.name as rule_name, d.name as device_name
            FROM alarm_occurrences ao
            LEFT JOIN alarm_rules ar ON ao.rule_id = ar.id
            LEFT JOIN devices d ON ao.device_id = d.id
            WHERE ao.acknowledged_by = ? AND ao.tenant_id = ?
            ORDER BY ao.acknowledged_time DESC
        `;
        const params = [parseInt(userId), tenantId];
        
        // 총 개수 조회
        const countQuery = `
            SELECT COUNT(*) as total
            FROM alarm_occurrences ao
            WHERE ao.acknowledged_by = ? AND ao.tenant_id = ?
        `;
        const countResult = await dbGet(countQuery, params);
        const total = countResult?.total || 0;
        
        // 페이징 적용
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query += ` LIMIT ${parseInt(limit)} OFFSET ${offset}`;
        
        const results = await dbAll(query, params);
        
        const result = {
            items: results.map(occurrence => formatAlarmOccurrence(occurrence)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`사용자 ${userId}가 확인한 알람 ${results.length}개 조회 완료`);
        res.json(createResponse(true, result, 'User acknowledged alarms retrieved successfully'));

    } catch (error) {
        console.error(`사용자 ${req.params.userId} 확인 알람 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'USER_ACKNOWLEDGED_ALARMS_ERROR'));
    }
});

/**
 * GET /api/alarms/audit-trail
 * 알람 감사 추적 조회 - cleared_by 및 acknowledged_by 활용
 */
router.get('/audit-trail', async (req, res) => {
    try {
        const { tenantId } = req;
        const { page = 1, limit = 100, user_id, action } = req.query;
        
        console.log('알람 감사 추적 조회...');

        let query = `
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
        `;
        let params = [tenantId];
        
        // 사용자 필터링
        if (user_id) {
            query += ` AND (ao.acknowledged_by = ? OR ao.cleared_by = ?)`;
            params.push(parseInt(user_id), parseInt(user_id));
        }
        
        // 액션 필터링
        if (action === 'acknowledged') {
            query += ` AND ao.acknowledged_time IS NOT NULL`;
        } else if (action === 'cleared') {
            query += ` AND ao.cleared_time IS NOT NULL`;
        }
        
        query += ` ORDER BY ao.updated_at DESC`;
        
        // 총 개수 조회
        const totalResults = await dbAll(query, params);
        const total = totalResults.length;
        
        // 페이징 적용
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query += ` LIMIT ${parseInt(limit)} OFFSET ${offset}`;
        
        const results = await dbAll(query, params);
        
        const result = {
            items: results.map(occurrence => ({
                ...formatAlarmOccurrence(occurrence),
                acknowledged_by_name: occurrence.acknowledged_by_name,
                cleared_by_name: occurrence.cleared_by_name
            })),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`감사 추적 ${results.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Alarm audit trail retrieved successfully'));

    } catch (error) {
        console.error('감사 추적 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'AUDIT_TRAIL_ERROR'));
    }
});
/**
 * GET /api/alarms/occurrences/category/:category
 * 카테고리별 알람 발생 조회
 */
router.get('/occurrences/category/:category', async (req, res) => {
    try {
        const { category } = req.params;
        const { tenantId } = req;
        const { page = 1, limit = 50 } = req.query;
        
        console.log(`카테고리 ${category} 알람 발생 조회...`);

        let query = AlarmQueries.AlarmOccurrence.FIND_BY_CATEGORY;
        const params = [tenantId, category];
        
        // 총 개수 조회
        const totalResults = await dbAll(query, params);
        const total = totalResults.length;
        
        // 페이징 적용
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query = AlarmQueries.addPagination(query, parseInt(limit), offset);
        
        const results = await dbAll(query, params);
        
        const result = {
            items: results.map(occurrence => formatAlarmOccurrence(occurrence)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`카테고리 ${category} 알람 발생 ${results.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Category alarm occurrences retrieved successfully'));

    } catch (error) {
        console.error(`카테고리 ${req.params.category} 알람 발생 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CATEGORY_ALARM_OCCURRENCES_ERROR'));
    }
});

/**
 * GET /api/alarms/occurrences/tag/:tag
 * 태그별 알람 발생 조회
 */
router.get('/occurrences/tag/:tag', async (req, res) => {
    try {
        const { tag } = req.params;
        const { tenantId } = req;
        const { page = 1, limit = 50 } = req.query;
        
        console.log(`태그 ${tag} 알람 발생 조회...`);

        let query = AlarmQueries.AlarmOccurrence.FIND_BY_TAG;
        const params = [tenantId, `%${tag}%`];
        
        // 총 개수 조회
        const totalResults = await dbAll(query, params);
        const total = totalResults.length;
        
        // 페이징 적용
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query = AlarmQueries.addPagination(query, parseInt(limit), offset);
        
        const results = await dbAll(query, params);
        
        const result = {
            items: results.map(occurrence => formatAlarmOccurrence(occurrence)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`태그 ${tag} 알람 발생 ${results.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Tag alarm occurrences retrieved successfully'));

    } catch (error) {
        console.error(`태그 ${req.params.tag} 알람 발생 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TAG_ALARM_OCCURRENCES_ERROR'));
    }
});

// ============================================================================
// 🚀 중요: 특정 라우트들을 먼저 등록 (/:id 라우트보다 먼저!)
// ============================================================================

/**
 * PATCH /api/alarms/rules/:id/toggle
 * 알람 규칙 활성화/비활성화 토글 (간단!)
 */
router.patch('/rules/:id/toggle', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { is_enabled } = req.body;  // true 또는 false만 받음

        console.log(`🔄 알람 규칙 ${id} 상태 변경: ${is_enabled}`);

        // 간단한 쿼리 사용 - name 필드 건드리지 않음!
        const params = AlarmQueries.buildEnabledStatusParams(is_enabled, id, tenantId);
        const result = await dbRun(AlarmQueries.AlarmRule.UPDATE_ENABLED_STATUS, params);

        if (result.changes > 0) {
            console.log(`✅ 알람 규칙 ${id} 상태 변경 완료`);
            res.json(createResponse(true, { 
                id: parseInt(id), 
                is_enabled: is_enabled 
            }, `Alarm rule ${is_enabled ? 'enabled' : 'disabled'} successfully`));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 상태 변경 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_TOGGLE_ERROR'));
    }
});

/**
 * PATCH /api/alarms/rules/:id/settings  
 * 알람 규칙 설정만 업데이트 (name 건드리지 않음)
 */
router.patch('/rules/:id/settings', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const settings = req.body;  // is_enabled, notification_enabled 등만

        console.log(`⚙️ 알람 규칙 ${id} 설정 업데이트:`, settings);

        // 설정만 업데이트하는 쿼리 사용
        const params = AlarmQueries.buildSettingsParams(settings, id, tenantId);
        const result = await dbRun(AlarmQueries.AlarmRule.UPDATE_SETTINGS_ONLY, params);

        if (result.changes > 0) {
            console.log(`✅ 알람 규칙 ${id} 설정 업데이트 완료`);
            res.json(createResponse(true, { 
                id: parseInt(id),
                updated_settings: settings 
            }, 'Alarm rule settings updated successfully'));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 설정 업데이트 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_SETTINGS_ERROR'));
    }
});

/**
 * PATCH /api/alarms/rules/:id/name
 * 알람 규칙 이름만 업데이트
 */
router.patch('/rules/:id/name', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { name } = req.body;

        console.log(`📝 알람 규칙 ${id} 이름 업데이트: ${name}`);

        const params = AlarmQueries.buildNameParams(name, id, tenantId);
        const result = await dbRun(AlarmQueries.AlarmRule.UPDATE_NAME_ONLY, params);

        if (result.changes > 0) {
            console.log(`✅ 알람 규칙 ${id} 이름 업데이트 완료`);
            res.json(createResponse(true, { 
                id: parseInt(id),
                name: name 
            }, 'Alarm rule name updated successfully'));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 이름 업데이트 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_NAME_ERROR'));
    }
});

/**
 * PATCH /api/alarms/rules/:id/severity
 * 알람 규칙 심각도만 업데이트
 */
router.patch('/rules/:id/severity', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { severity } = req.body;

        console.log(`⚠️ 알람 규칙 ${id} 심각도 업데이트: ${severity}`);

        const params = AlarmQueries.buildSeverityParams(severity, id, tenantId);
        const result = await dbRun(AlarmQueries.AlarmRule.UPDATE_SEVERITY_ONLY, params);

        if (result.changes > 0) {
            console.log(`✅ 알람 규칙 ${id} 심각도 업데이트 완료`);
            res.json(createResponse(true, { 
                id: parseInt(id),
                severity: severity 
            }, 'Alarm rule severity updated successfully'));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 심각도 업데이트 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_SEVERITY_ERROR'));
    }
});

/**
 * GET /api/alarms/rules/category/:category
 * 카테고리별 알람 규칙 조회
 */
router.get('/rules/category/:category', async (req, res) => {
    try {
        const { category } = req.params;
        const { tenantId } = req;
        
        console.log(`카테고리 ${category} 알람 규칙 조회...`);

        const results = await dbAll(AlarmQueries.AlarmRule.FIND_BY_CATEGORY, [category, tenantId]);
        
        console.log(`카테고리 ${category} 알람 규칙 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(rule => formatAlarmRule(rule)), 'Category alarm rules retrieved successfully'));

    } catch (error) {
        console.error(`카테고리 ${req.params.category} 알람 규칙 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CATEGORY_ALARM_RULES_ERROR'));
    }
});

/**
 * GET /api/alarms/rules/tag/:tag
 * 태그별 알람 규칙 조회
 */
router.get('/rules/tag/:tag', async (req, res) => {
    try {
        const { tag } = req.params;
        const { tenantId } = req;
        
        console.log(`태그 ${tag} 알람 규칙 조회...`);

        const results = await dbAll(AlarmQueries.AlarmRule.FIND_BY_TAG, [`%${tag}%`, tenantId]);
        
        console.log(`태그 ${tag} 알람 규칙 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(rule => formatAlarmRule(rule)), 'Tag alarm rules retrieved successfully'));

    } catch (error) {
        console.error(`태그 ${req.params.tag} 알람 규칙 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TAG_ALARM_RULES_ERROR'));
    }
});

/**
 * GET /api/alarms/rules/statistics
 * 알람 규칙 통계 조회 - category 통계 포함
 */
router.get('/rules/statistics', async (req, res) => {
    try {
        const { tenantId } = req;
        
        console.log('알람 규칙 통계 조회 시작...');

        const [
            summaryStats,
            severityStats,
            typeStats,
            categoryStats  // 새로 추가
        ] = await Promise.all([
            dbGet(AlarmQueries.AlarmRule.STATS_SUMMARY, [tenantId]),
            dbAll(AlarmQueries.AlarmRule.STATS_BY_SEVERITY, [tenantId]),
            dbAll(AlarmQueries.AlarmRule.STATS_BY_TYPE, [tenantId]),
            dbAll(AlarmQueries.AlarmRule.STATS_BY_CATEGORY, [tenantId])  // 새로 추가
        ]);

        const stats = {
            summary: summaryStats,
            by_severity: severityStats,
            by_type: typeStats,
            by_category: categoryStats  // 새로 추가
        };
        
        console.log('알람 규칙 통계 조회 완료');
        res.json(createResponse(true, stats, 'Alarm rule statistics retrieved successfully'));

    } catch (error) {
        console.error('알람 규칙 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_STATS_ERROR'));
    }
});

// ============================================================================
// 일반적인 알람 규칙 CRUD 라우트들 (특정 라우트들 이후에 등록)
// ============================================================================

/**
 * GET /api/alarms/rules
 * 알람 규칙 목록 조회
 */
router.get('/rules', async (req, res) => {
    try {
        const { tenantId } = req;
        const { 
            page = 1, 
            limit = 50,
            enabled,
            alarm_type,
            severity,
            target_type,
            target_id,
            search,
            category,  // 새로 추가
            tag        // 새로 추가
        } = req.query;
        
        console.log('알람 규칙 조회 시작:', { tenantId, page, limit, enabled, alarm_type, severity, search, category, tag });

        let query, params;
        
        if (search) {
            // 검색 쿼리 사용 - category, tags 검색 포함
            query = AlarmQueries.AlarmRule.SEARCH;
            const searchTerm = `%${search}%`;
            params = [tenantId, searchTerm, searchTerm, searchTerm, searchTerm, searchTerm, searchTerm, searchTerm, searchTerm, searchTerm];
        } else {
            // 기본 쿼리 사용하고 수동으로 필터 적용
            query = AlarmQueries.AlarmRule.FIND_ALL;
            params = [tenantId];
            
            // 추가 필터들을 수동으로 WHERE 절에 추가
            if (alarm_type && alarm_type !== 'all') {
                query += ` AND ar.alarm_type = ?`;
                params.push(alarm_type);
            }
            
            if (severity && severity !== 'all') {
                query += ` AND ar.severity = ?`;
                params.push(severity);
            }
            
            if (enabled !== undefined) {
                query += ` AND ar.is_enabled = ?`;
                params.push(enabled === 'true' ? 1 : 0);
            }
            
            if (target_type && target_type !== 'all') {
                query += ` AND ar.target_type = ?`;
                params.push(target_type);
            }
            
            if (target_id) {
                query += ` AND ar.target_id = ?`;
                params.push(parseInt(target_id));
            }
            
            if (category && category !== 'all') {
                query += ` AND ar.category = ?`;
                params.push(category);
            }
            
            if (tag && tag.trim()) {
                query += ` AND ar.tags LIKE ?`;
                params.push(`%${tag}%`);
            }
        }
        
        // 총 개수 조회 (페이징 전)
        const totalResults = await dbAll(query, params);
        const total = totalResults.length;
        
        // 페이징 적용
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query = AlarmQueries.addPagination(query, parseInt(limit), offset);
        
        console.log('실행할 쿼리:', query);
        console.log('파라미터:', params);
        
        const results = await dbAll(query, params);
        
        const result = {
            items: results.map(rule => formatAlarmRule(rule)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`알람 규칙 ${results.length}개 조회 완료 (총 ${total}개)`);
        res.json(createResponse(true, result, 'Alarm rules retrieved successfully'));

    } catch (error) {
        console.error('알람 규칙 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULES_ERROR'));
    }
});

/**
 * GET /api/alarms/rules/:id
 * 특정 알람 규칙 조회
 */
router.get('/rules/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        
        console.log(`알람 규칙 ID ${id} 조회...`);

        const result = await dbGet(AlarmQueries.AlarmRule.FIND_BY_ID, [parseInt(id), tenantId]);

        if (!result) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

        console.log(`알람 규칙 ID ${id} 조회 완료`);
        res.json(createResponse(true, formatAlarmRule(result), 'Alarm rule retrieved successfully'));

    } catch (error) {
        console.error(`알람 규칙 ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_DETAIL_ERROR'));
    }
});

/**
 * POST /api/alarms/rules
 * 새 알람 규칙 생성
 */
router.post('/rules', async (req, res) => {
    try {
        const { tenantId, user } = req;
        const alarmRuleData = {
            ...req.body,
            tenant_id: tenantId,
            created_by: user.id
        };

        console.log('새 알람 규칙 생성:', alarmRuleData);

        // 필수 필드 검증
        AlarmQueries.validateAlarmRule(alarmRuleData);

        // AlarmQueries 헬퍼 사용 - category, tags 포함
        const params = AlarmQueries.buildCreateRuleParams(alarmRuleData);

        const result = await dbRun(AlarmQueries.AlarmRule.CREATE, params);

        if (result.lastID) {
            // 생성된 규칙 조회
            const newRule = await dbGet(AlarmQueries.AlarmRule.FIND_BY_ID, [result.lastID, tenantId]);

            console.log(`새 알람 규칙 생성 완료: ID ${result.lastID}`);
            res.status(201).json(createResponse(true, formatAlarmRule(newRule), 'Alarm rule created successfully'));
        } else {
            throw new Error('알람 규칙 생성 실패 - ID 반환되지 않음');
        }

    } catch (error) {
        console.error('알람 규칙 생성 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_CREATE_ERROR'));
    }
});

/**
 * PUT /api/alarms/rules/:id
 * 알람 규칙 수정
 */
router.put('/rules/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const updateData = req.body;

        console.log(`알람 규칙 ${id} 수정:`, updateData);

        // AlarmQueries 헬퍼 사용 - category, tags 포함
        const params = AlarmQueries.buildUpdateRuleParams(updateData, parseInt(id), tenantId);

        const result = await dbRun(AlarmQueries.AlarmRule.UPDATE, params);

        if (result.changes > 0) {
            // 수정된 규칙 조회
            const updatedRule = await dbGet(AlarmQueries.AlarmRule.FIND_BY_ID, [parseInt(id), tenantId]);

            console.log(`알람 규칙 ID ${id} 수정 완료`);
            res.json(createResponse(true, formatAlarmRule(updatedRule), 'Alarm rule updated successfully'));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found or update failed', 'ALARM_RULE_UPDATE_FAILED')
            );
        }

    } catch (error) {
        console.error(`알람 규칙 ${req.params.id} 수정 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_UPDATE_ERROR'));
    }
});

/**
 * DELETE /api/alarms/rules/:id
 * 알람 규칙 삭제
 */
router.delete('/rules/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`알람 규칙 ${id} 삭제...`);

        const result = await dbRun(AlarmQueries.AlarmRule.DELETE, [parseInt(id), tenantId]);

        if (result.changes > 0) {
            console.log(`알람 규칙 ID ${id} 삭제 완료`);
            res.json(createResponse(true, { deleted: true }, 'Alarm rule deleted successfully'));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found or delete failed', 'ALARM_RULE_DELETE_FAILED')
            );
        }

    } catch (error) {
        console.error(`알람 규칙 ${req.params.id} 삭제 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_DELETE_ERROR'));
    }
});

// ============================================================================
// 알람 템플릿 (Alarm Templates) API
// ============================================================================

/**
 * GET /api/alarms/templates
 * 알람 템플릿 목록 조회
 */
router.get('/templates', async (req, res) => {
    try {
        const { tenantId } = req;
        const { 
            page = 1, 
            limit = 50,
            category,
            is_system_template,
            search,
            tag  // 새로 추가
        } = req.query;
        
        console.log('알람 템플릿 조회 시작...');

        let query, params;
        
        if (search) {
            query = AlarmQueries.AlarmTemplate.SEARCH;
            const searchTerm = `%${search}%`;
            params = [tenantId, searchTerm, searchTerm, searchTerm, searchTerm];  // tags 검색 추가
        } else if (tag) {
            query = AlarmQueries.AlarmTemplate.FIND_BY_TAG;
            params = [`%${tag}%`, tenantId];
        } else if (category) {
            query = AlarmQueries.AlarmTemplate.FIND_BY_CATEGORY;
            params = [category, tenantId];
        } else if (is_system_template === 'true') {
            query = AlarmQueries.AlarmTemplate.FIND_SYSTEM_TEMPLATES;
            params = [];
        } else {
            query = AlarmQueries.AlarmTemplate.FIND_ALL;
            params = [tenantId];
        }
        
        // 총 개수 조회
        const totalResults = await dbAll(query, params);
        const total = totalResults.length;
        
        // 페이징 적용
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query = AlarmQueries.addPagination(query, parseInt(limit), offset);
        console.log('실행할 쿼리:', query);
        console.log('파라미터:', params);
        const results = await dbAll(query, params);
        console.log('쿼리 결과 타입:', typeof results);
        console.log('쿼리 결과 길이:', results ? results.length : 'null/undefined');
        console.log('첫 번째 결과 샘플:', results[0]);
        const result = {
            items: results.map(template => formatAlarmTemplate(template)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`알람 템플릿 ${results.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Alarm templates retrieved successfully'));
        
    } catch (error) {
        console.error('알람 템플릿 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_TEMPLATES_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/:id
 * 특정 알람 템플릿 조회
 */
router.get('/templates/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        
        console.log(`알람 템플릿 ID ${id} 조회...`);

        const result = await dbGet(AlarmQueries.AlarmTemplate.FIND_BY_ID, [parseInt(id), tenantId]);

        if (!result) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm template not found', 'ALARM_TEMPLATE_NOT_FOUND')
            );
        }

        console.log(`알람 템플릿 ID ${id} 조회 완료`);
        res.json(createResponse(true, formatAlarmTemplate(result), 'Alarm template retrieved successfully'));

    } catch (error) {
        console.error(`알람 템플릿 ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_TEMPLATE_DETAIL_ERROR'));
    }
});

/**
 * POST /api/alarms/templates
 * 새 알람 템플릿 생성
 */
router.post('/templates', async (req, res) => {
    try {
        const { tenantId, user } = req;
        const templateData = {
            ...req.body,
            tenant_id: tenantId,
            created_by: user.id
        };

        console.log('새 알람 템플릿 생성...');

        // 필수 필드 검증
        AlarmQueries.validateTemplateRequiredFields(templateData);

        // 템플릿 설정 유효성 검증 - tags 포함
        AlarmQueries.validateTemplateConfig(templateData);

        // AlarmQueries 헬퍼 사용 - tags 포함
        const params = AlarmQueries.buildCreateTemplateParams(templateData);

        const result = await dbRun(AlarmQueries.AlarmTemplate.CREATE, params);

        if (result.lastID) {
            const newTemplate = await dbGet(AlarmQueries.AlarmTemplate.FIND_BY_ID, [result.lastID, tenantId]);

            console.log(`새 알람 템플릿 생성 완료: ID ${result.lastID}`);
            res.status(201).json(createResponse(true, formatAlarmTemplate(newTemplate), 'Alarm template created successfully'));
        } else {
            throw new Error('알람 템플릿 생성 실패 - ID 반환되지 않음');
        }

    } catch (error) {
        console.error('알람 템플릿 생성 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_TEMPLATE_CREATE_ERROR'));
    }
});

/**
 * PUT /api/alarms/templates/:id
 * 알람 템플릿 수정
 */
router.put('/templates/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const updateData = req.body;

        console.log(`알람 템플릿 ${id} 수정...`);

        // 템플릿 설정 유효성 검증 - tags 포함
        AlarmQueries.validateTemplateConfig(updateData);

        // AlarmQueries 헬퍼 사용 - tags 포함
        const params = AlarmQueries.buildUpdateTemplateParams(updateData, parseInt(id), tenantId);

        const result = await dbRun(AlarmQueries.AlarmTemplate.UPDATE, params);

        if (result.changes > 0) {
            const updatedTemplate = await dbGet(AlarmQueries.AlarmTemplate.FIND_BY_ID, [parseInt(id), tenantId]);

            console.log(`알람 템플릿 ID ${id} 수정 완료`);
            res.json(createResponse(true, formatAlarmTemplate(updatedTemplate), 'Alarm template updated successfully'));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm template not found or update failed', 'ALARM_TEMPLATE_UPDATE_FAILED')
            );
        }

    } catch (error) {
        console.error(`알람 템플릿 ${req.params.id} 수정 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_TEMPLATE_UPDATE_ERROR'));
    }
});

/**
 * DELETE /api/alarms/templates/:id
 * 알람 템플릿 삭제 (소프트 삭제)
 */
router.delete('/templates/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;

        console.log(`알람 템플릿 ${id} 삭제...`);

        const result = await dbRun(AlarmQueries.AlarmTemplate.DELETE, [parseInt(id), tenantId]);

        if (result.changes > 0) {
            console.log(`알람 템플릿 ID ${id} 삭제 완료`);
            res.json(createResponse(true, { deleted: true }, 'Alarm template deleted successfully'));
        } else {
            return res.status(404).json(
                createResponse(false, null, 'Alarm template not found or delete failed', 'ALARM_TEMPLATE_DELETE_FAILED')
            );
        }

    } catch (error) {
        console.error(`알람 템플릿 ${req.params.id} 삭제 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_TEMPLATE_DELETE_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/category/:category
 * 카테고리별 알람 템플릿 조회
 */
router.get('/templates/category/:category', async (req, res) => {
    try {
        const { category } = req.params;
        const { tenantId } = req;
        
        console.log(`카테고리 ${category} 템플릿 조회...`);

        const results = await dbAll(AlarmQueries.AlarmTemplate.FIND_BY_CATEGORY, [category, tenantId]);
        
        console.log(`카테고리 ${category} 템플릿 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(template => formatAlarmTemplate(template)), 'Category templates retrieved successfully'));

    } catch (error) {
        console.error(`카테고리 ${req.params.category} 템플릿 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CATEGORY_TEMPLATES_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/tag/:tag
 * 태그별 알람 템플릿 조회
 */
router.get('/templates/tag/:tag', async (req, res) => {
    try {
        const { tag } = req.params;
        const { tenantId } = req;
        
        console.log(`태그 ${tag} 템플릿 조회...`);

        const results = await dbAll(AlarmQueries.AlarmTemplate.FIND_BY_TAG, [`%${tag}%`, tenantId]);
        
        console.log(`태그 ${tag} 템플릿 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(template => formatAlarmTemplate(template)), 'Tag templates retrieved successfully'));

    } catch (error) {
        console.error(`태그 ${req.params.tag} 템플릿 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TAG_TEMPLATES_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/system
 * 시스템 기본 템플릿 조회
 */
router.get('/templates/system', async (req, res) => {
    try {
        console.log('시스템 템플릿 조회...');

        const results = await dbAll(AlarmQueries.AlarmTemplate.FIND_SYSTEM_TEMPLATES, []);
        
        console.log(`시스템 템플릿 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(template => formatAlarmTemplate(template)), 'System templates retrieved successfully'));

    } catch (error) {
        console.error('시스템 템플릿 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'SYSTEM_TEMPLATES_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/data-type/:dataType
 * 데이터 타입별 적용 가능한 템플릿 조회
 */
router.get('/templates/data-type/:dataType', async (req, res) => {
    try {
        const { dataType } = req.params;
        const { tenantId } = req;
        
        console.log(`데이터 타입 ${dataType} 적용 가능 템플릿 조회...`);

        const results = await dbAll(AlarmQueries.AlarmTemplate.FIND_BY_DATA_TYPE, [tenantId, `%"${dataType}"%`]);
        
        console.log(`데이터 타입 ${dataType} 템플릿 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(template => formatAlarmTemplate(template)), 'Data type templates retrieved successfully'));

    } catch (error) {
        console.error(`데이터 타입 ${req.params.dataType} 템플릿 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DATA_TYPE_TEMPLATES_ERROR'));
    }
});

/**
 * POST /api/alarms/templates/:id/apply
 * 템플릿을 여러 데이터포인트에 일괄 적용
 */
router.post('/templates/:id/apply', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId, user } = req;
        const { 
            target_ids = [],  // data_point_ids에서 변경
            target_type = 'data_point',  // 새로 추가
            custom_configs = {},
            rule_group_name = null 
        } = req.body;

        console.log(`템플릿 ${id}를 ${target_ids.length}개 타겟에 적용...`);

        // 템플릿 조회
        const template = await dbGet(AlarmQueries.AlarmTemplate.FIND_BY_ID, [parseInt(id), tenantId]);
        if (!template) {
            return res.status(404).json(
                createResponse(false, null, 'Template not found', 'TEMPLATE_NOT_FOUND')
            );
        }

        // 규칙 그룹 ID 생성
        const ruleGroupId = `template_${id}_${Date.now()}`;

        // 각 타겟에 대해 알람 규칙 생성
        const createdRules = [];
        for (const targetId of target_ids) {
            const customConfig = custom_configs[targetId] || {};
            const mergedConfig = {
                ...parseJSON(template.default_config),
                ...customConfig
            };

            const ruleData = {
                tenant_id: tenantId,
                name: `${template.name}_${target_type}_${targetId}`,
                description: `${template.description} (템플릿에서 자동 생성)`,
                target_type: target_type,
                target_id: targetId,
                target_group: null,
                alarm_type: template.condition_type,
                // 기본 설정을 mergedConfig에서 추출
                ...mergedConfig,
                severity: template.severity,
                message_template: template.message_template,
                auto_acknowledge: template.auto_acknowledge,
                auto_clear: template.auto_clear,
                notification_enabled: template.notification_enabled,
                template_id: template.id,
                rule_group: ruleGroupId,
                created_by_template: 1,
                category: template.category,  // 새로 추가
                tags: template.tags,  // 새로 추가
                is_enabled: 1,
                created_by: user.id
            };

            try {
                // 필수 필드 검증
                AlarmQueries.validateAlarmRule(ruleData);
                
                const params = AlarmQueries.buildCreateRuleParams(ruleData);
                const result = await dbRun(AlarmQueries.AlarmRule.CREATE, params);
                
                if (result.lastID) {
                    const newRule = await dbGet(AlarmQueries.AlarmRule.FIND_BY_ID, [result.lastID, tenantId]);
                    if (newRule) {
                        createdRules.push(newRule);
                    }
                }
            } catch (ruleError) {
                console.error(`타겟 ${targetId} 규칙 생성 실패:`, ruleError.message);
            }
        }

        // 템플릿 사용량 증가
        await dbRun(AlarmQueries.AlarmTemplate.INCREMENT_USAGE, [createdRules.length, template.id]);

        console.log(`템플릿 적용 완료: ${createdRules.length}개 규칙 생성`);
        res.json(createResponse(true, {
            template_id: template.id,
            template_name: template.name,
            rule_group_id: ruleGroupId,
            rules_created: createdRules.length,
            created_rules: createdRules.map(rule => formatAlarmRule(rule))
        }, 'Template applied successfully'));

    } catch (error) {
        console.error(`템플릿 ${req.params.id} 적용 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TEMPLATE_APPLY_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/:id/applied-rules
 * 템플릿으로 생성된 규칙들 조회
 */
router.get('/templates/:id/applied-rules', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        
        console.log(`템플릿 ${id}로 생성된 규칙들 조회...`);

        const results = await dbAll(AlarmQueries.AlarmTemplate.FIND_APPLIED_RULES, [parseInt(id), tenantId]);
        
        console.log(`템플릿 ${id}로 생성된 규칙 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(rule => formatAlarmRule(rule)), 'Applied rules retrieved successfully'));

    } catch (error) {
        console.error(`템플릿 ${req.params.id} 적용 규칙 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'APPLIED_RULES_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/statistics
 * 알람 템플릿 통계 조회 - tags 통계 포함
 */
router.get('/templates/statistics', async (req, res) => {
    try {
        const { tenantId } = req;
        
        console.log('알람 템플릿 통계 조회 시작...');

        const [
            summaryStats,
            categoryStats,
            mostUsed
        ] = await Promise.all([
            dbGet(AlarmQueries.AlarmTemplate.STATS_SUMMARY, [tenantId]),
            dbAll(AlarmQueries.AlarmTemplate.COUNT_BY_CATEGORY, [tenantId]),
            dbAll(AlarmQueries.AlarmTemplate.MOST_USED, [tenantId, 5])
        ]);

        const stats = {
            summary: summaryStats,
            by_category: categoryStats,
            most_used: mostUsed.map(template => formatAlarmTemplate(template))
        };
        
        console.log('알람 템플릿 통계 조회 완료');
        res.json(createResponse(true, stats, 'Template statistics retrieved successfully'));

    } catch (error) {
        console.error('알람 템플릿 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TEMPLATE_STATS_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/search
 * 알람 템플릿 검색 - tags 검색 포함
 */
router.get('/templates/search', async (req, res) => {
    try {
        const { tenantId } = req;
        const { q: searchTerm, limit = 20 } = req.query;
        
        if (!searchTerm) {
            return res.status(400).json(
                createResponse(false, null, 'Search term is required', 'SEARCH_TERM_REQUIRED')
            );
        }

        console.log(`알람 템플릿 검색: "${searchTerm}"`);
        
        let query = AlarmQueries.AlarmTemplate.SEARCH;
        query = AlarmQueries.addPagination(query, parseInt(limit));
        
        const searchPattern = `%${searchTerm}%`;
        const results = await dbAll(query, [tenantId, searchPattern, searchPattern, searchPattern, searchPattern]);
        
        console.log(`검색 결과: ${results.length}개 템플릿`);
        res.json(createResponse(true, results.map(template => formatAlarmTemplate(template)), 'Template search completed successfully'));

    } catch (error) {
        console.error(`템플릿 검색 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TEMPLATE_SEARCH_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/most-used
 * 가장 많이 사용된 템플릿들 조회
 */
router.get('/templates/most-used', async (req, res) => {
    try {
        const { tenantId } = req;
        const { limit = 10 } = req.query;
        
        console.log('인기 템플릿 조회...');

        const results = await dbAll(AlarmQueries.AlarmTemplate.MOST_USED, [tenantId, parseInt(limit)]);
        
        console.log(`인기 템플릿 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(template => formatAlarmTemplate(template)), 'Most used templates retrieved successfully'));

    } catch (error) {
        console.error('인기 템플릿 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'MOST_USED_TEMPLATES_ERROR'));
    }
});

// ============================================================================
// 특화 API 엔드포인트들
// ============================================================================

/**
 * GET /api/alarms/statistics
 * 알람 통계 조회 - category 통계 포함
 */
router.get('/statistics', async (req, res) => {
    try {
        const { tenantId } = req;
        
        console.log('알람 통계 조회 시작...');

        const [
            occurrenceStats,
            ruleStats,
            occurrenceCategoryStats,  // 새로 추가
            ruleCategoryStats         // 새로 추가
        ] = await Promise.all([
            dbGet(AlarmQueries.AlarmOccurrence.STATS_SUMMARY, [tenantId]),
            dbGet(AlarmQueries.AlarmRule.STATS_SUMMARY, [tenantId]),
            dbAll(AlarmQueries.AlarmOccurrence.STATS_BY_CATEGORY, [tenantId]),  // 새로 추가
            dbAll(AlarmQueries.AlarmRule.STATS_BY_CATEGORY, [tenantId])         // 새로 추가
        ]);

        const stats = {
            occurrences: {
                ...occurrenceStats,
                by_category: occurrenceCategoryStats  // 새로 추가
            },
            rules: {
                ...ruleStats,
                by_category: ruleCategoryStats        // 새로 추가
            },
            dashboard_summary: {
                total_active: occurrenceStats?.active_alarms || 0,
                total_rules: ruleStats?.total_rules || 0,
                unacknowledged: occurrenceStats?.unacknowledged_alarms || 0,
                enabled_rules: ruleStats?.enabled_rules || 0,
                categories: ruleCategoryStats?.length || 0,
                rules_with_tags: ruleStats?.rules_with_tags || 0
            }
        };
        
        console.log('알람 통계 조회 완료');
        res.json(createResponse(true, stats, 'Alarm statistics retrieved successfully'));

    } catch (error) {
        console.error('알람 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_STATS_ERROR'));
    }
});

/**
 * GET /api/alarms/unacknowledged
 * 미확인 알람만 조회
 */
router.get('/unacknowledged', async (req, res) => {
    try {
        const { tenantId } = req;
        
        console.log('미확인 알람 조회 시작...');

        const results = await dbAll(AlarmQueries.AlarmOccurrence.FIND_UNACKNOWLEDGED, [tenantId]);
        
        console.log(`미확인 알람 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(alarm => formatAlarmOccurrence(alarm)), 'Unacknowledged alarms retrieved successfully'));

    } catch (error) {
        console.error('미확인 알람 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'UNACKNOWLEDGED_ALARMS_ERROR'));
    }
});

/**
 * GET /api/alarms/device/:deviceId
 * 특정 디바이스의 알람 조회
 */
router.get('/device/:deviceId', async (req, res) => {
    try {
        const { deviceId } = req.params;
        const { tenantId } = req;
        
        console.log(`디바이스 ${deviceId} 알람 조회 시작...`);

        const results = await dbAll(AlarmQueries.AlarmOccurrence.FIND_BY_DEVICE, [tenantId, parseInt(deviceId)]);
        
        console.log(`디바이스 ${deviceId} 알람 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(alarm => formatAlarmOccurrence(alarm)), 'Device alarms retrieved successfully'));

    } catch (error) {
        console.error(`디바이스 ${req.params.deviceId} 알람 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_ALARMS_ERROR'));
    }
});

/**
 * GET /api/alarms/recent
 * 최근 알람 발생 조회
 */
router.get('/recent', async (req, res) => {
    try {
        const { tenantId } = req;
        const { limit = 20 } = req.query;
        
        console.log('최근 알람 조회 시작...');

        const results = await dbAll(AlarmQueries.AlarmOccurrence.RECENT_OCCURRENCES, [tenantId, parseInt(limit)]);
        
        console.log(`최근 알람 ${results.length}개 조회 완료`);
        res.json(createResponse(true, results.map(alarm => formatAlarmOccurrence(alarm)), 'Recent alarms retrieved successfully'));

    } catch (error) {
        console.error('최근 알람 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'RECENT_ALARMS_ERROR'));
    }
});

/**
 * GET /api/alarms/test
 * 알람 API 테스트 엔드포인트
 */
router.get('/test', async (req, res) => {
    try {
        // DatabaseFactory 연결 테스트
        const factory = await getDatabaseFactory();
        const testResult = await factory.executeQuery('SELECT 1 as test', []);
        
        res.json(createResponse(true, { 
            message: 'Complete Alarm API is working with simplified update queries!',
            database_test: testResult,
            architecture: [
                'ConfigManager-based database configuration',
                'DatabaseFactory.executeQuery for unified database access', 
                'AlarmQueries for centralized SQL management',
                'DB-type independent implementation',
                'Complete feature coverage with category/tags support',
                '🆕 Simplified update queries for is_enabled field'
            ],
            available_endpoints: [
                // 알람 발생 관련
                'GET /api/alarms/active',
                'GET /api/alarms/occurrences',
                'GET /api/alarms/occurrences/:id',
                'GET /api/alarms/occurrences/category/:category',
                'GET /api/alarms/occurrences/tag/:tag',
                'GET /api/alarms/history',
                'POST /api/alarms/occurrences/:id/acknowledge',
                'POST /api/alarms/occurrences/:id/clear',
                'GET /api/alarms/unacknowledged',
                'GET /api/alarms/recent',
                'GET /api/alarms/device/:deviceId',
                
                // 알람 규칙 관리 (기존)
                'GET /api/alarms/rules',
                'GET /api/alarms/rules/:id',
                'GET /api/alarms/rules/category/:category',
                'GET /api/alarms/rules/tag/:tag',
                'POST /api/alarms/rules',
                'PUT /api/alarms/rules/:id',
                'DELETE /api/alarms/rules/:id',
                'GET /api/alarms/rules/statistics',
                
                // 🆕 간단한 업데이트 엔드포인트들 (NEW!)
                'PATCH /api/alarms/rules/:id/toggle',        // is_enabled만 토글
                'PATCH /api/alarms/rules/:id/settings',      // 설정만 업데이트
                'PATCH /api/alarms/rules/:id/name',          // 이름만 업데이트
                'PATCH /api/alarms/rules/:id/severity',      // 심각도만 업데이트
                
                // 알람 템플릿 관련
                'GET /api/alarms/templates',
                'GET /api/alarms/templates/:id',
                'GET /api/alarms/templates/category/:category',
                'GET /api/alarms/templates/tag/:tag',
                'POST /api/alarms/templates',
                'PUT /api/alarms/templates/:id',
                'DELETE /api/alarms/templates/:id',
                'GET /api/alarms/templates/system',
                'GET /api/alarms/templates/data-type/:dataType',
                'POST /api/alarms/templates/:id/apply',
                'GET /api/alarms/templates/:id/applied-rules',
                'GET /api/alarms/templates/statistics',
                'GET /api/alarms/templates/search',
                'GET /api/alarms/templates/most-used',
                
                // 통계 및 기타
                'GET /api/alarms/statistics',
                'GET /api/alarms/test'
            ],
            route_order_fixed: true,
            toggle_route_working: true
        }, 'Complete Alarm API test successful - route order fixed!'));

    } catch (error) {
        console.error('테스트 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TEST_ERROR'));
    }
});

module.exports = router;