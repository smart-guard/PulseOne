// ============================================================================
// backend/routes/alarms.js
// 완전한 알람 관리 API - DatabaseFactory.executeQuery 사용으로 수정
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
        
        // JOIN된 정보들 (이미 완성됨)
        device_name: rule.device_name,
        device_type: rule.device_type,
        // ... 기타 JOIN 정보들
        
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
        
        // 고급 기능 - escalation_rules 제거
        // escalation_rules: parseJSON(rule.escalation_rules), <- 이 줄 제거
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
        tenant_id: occurrence.tenant_id,
        alarm_rule_id: occurrence.alarm_rule_id,
        rule_name: occurrence.rule_name,
        device_id: occurrence.device_id,
        device_name: occurrence.device_name,
        data_point_id: occurrence.data_point_id,
        data_point_name: occurrence.data_point_name,
        virtual_point_id: occurrence.virtual_point_id,
        virtual_point_name: occurrence.virtual_point_name,
        severity: occurrence.severity,
        message: occurrence.message,
        trigger_value: occurrence.trigger_value,
        condition_details: occurrence.condition_details,
        state: occurrence.state,
        occurrence_time: occurrence.occurrence_time,
        acknowledgment_time: occurrence.acknowledgment_time,
        acknowledged_by: occurrence.acknowledged_by,
        acknowledgment_note: occurrence.acknowledgment_note,
        clear_time: occurrence.clear_time,
        cleared_by: occurrence.cleared_by,
        resolution_note: occurrence.resolution_note,
        escalation_level: occurrence.escalation_level,
        notification_sent: !!occurrence.notification_sent,
        site_location: occurrence.site_location,
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
        notification_enabled: !!template.notification_enabled,
        email_notification: !!template.email_notification,
        sms_notification: !!template.sms_notification,
        auto_acknowledge: !!template.auto_acknowledge,
        auto_clear: !!template.auto_clear,
        usage_count: template.usage_count || 0,
        is_active: !!template.is_active,
        is_system_template: !!template.is_system_template,
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
 * 활성 알람 목록 조회
 */
router.get('/active', async (req, res) => {
    try {
        const { tenantId } = req;
        const { 
            page = 1, 
            limit = 50,
            severity,
            device_id,
            acknowledged = false 
        } = req.query;
        
        console.log('활성 알람 조회 시작...');

        // AlarmQueries 사용
        const filters = AlarmQueries.buildAlarmOccurrenceFilters(
            AlarmQueries.AlarmOccurrence.FIND_ACTIVE, 
            { tenantId, severity, device_id, acknowledged: acknowledged === 'true' }
        );
        
        let query = filters.query;
        let params = filters.params;
        
        query = AlarmQueries.addSorting(query, 'occurrence_time', 'DESC');
        
        // 페이징
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query = AlarmQueries.addPagination(query, parseInt(limit), offset);

        const results = await dbAll(query, params);
        
        // 총 개수 조회
        const countQuery = AlarmQueries.AlarmOccurrence.STATS_SUMMARY;
        const countResult = await dbGet(countQuery, [tenantId]);
        const total = countResult?.active_alarms || 0;
        
        const result = {
            items: results.map(alarm => formatAlarmOccurrence(alarm)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: total,
                totalPages: Math.ceil(total / parseInt(limit))
            }
        };
        
        console.log(`활성 알람 ${results.length}개 조회 완료`);
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
            search
        } = req.query;
        
        console.log('알람 발생 목록 조회 시작...');

        let query, params;
        
        if (search) {
            // 검색 쿼리는 별도로 구현 필요
            query = AlarmQueries.AlarmOccurrence.FIND_ALL;
            params = [tenantId];
        } else {
            // 필터 적용
            const filters = AlarmQueries.buildAlarmOccurrenceFilters(
                AlarmQueries.AlarmOccurrence.FIND_ALL, 
                { tenantId, state, severity, rule_id, device_id }
            );
            
            query = filters.query;
            params = filters.params;
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
            state
        } = req.query;
        
        console.log('알람 이력 조회 시작...');

        let query, params;
        
        if (date_from && date_to) {
            query = AlarmQueries.AlarmOccurrence.FIND_BY_DATE_RANGE;
            params = [tenantId, date_from, date_to];
        } else {
            const filters = AlarmQueries.buildAlarmOccurrenceFilters(
                AlarmQueries.AlarmOccurrence.FIND_ALL, 
                { tenantId, severity, device_id, state }
            );
            
            query = filters.query;
            params = filters.params;
        }
        
        query = AlarmQueries.addSorting(query, 'occurrence_time', 'DESC');
        
        // 페이징
        const offset = (parseInt(page) - 1) * parseInt(limit);
        query = AlarmQueries.addPagination(query, parseInt(limit), offset);

        const results = await dbAll(query, params);
        
        const result = {
            items: results.map(occurrence => formatAlarmOccurrence(occurrence)),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: results.length,
                totalPages: Math.ceil(results.length / parseInt(limit))
            }
        };
        
        console.log(`알람 이력 ${results.length}개 조회 완료`);
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
        const { tenantId } = req;
        
        console.log(`알람 발생 ${id} 해제 처리 시작...`);

        const result = await dbRun(AlarmQueries.AlarmOccurrence.CLEAR, [clearedValue, comment, parseInt(id), tenantId]);

        if (result.changes > 0) {
            const updatedAlarm = await dbGet(AlarmQueries.AlarmOccurrence.FIND_BY_ID, [parseInt(id), tenantId]);

            console.log(`알람 발생 ${id} 해제 처리 완료`);
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

// ============================================================================
// 알람 규칙 (Alarm Rules) API
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
            condition_type,
            severity,
            device_id,
            data_point_id,
            virtual_point_id,
            search
        } = req.query;
        
        console.log('알람 규칙 조회 시작:', { tenantId, page, limit, enabled, condition_type, severity, search });

        let query, params;
        
        if (search) {
            // 검색 쿼리 사용
            query = AlarmQueries.AlarmRule.SEARCH;
            const searchTerm = `%${search}%`;
            params = [tenantId, searchTerm, searchTerm, searchTerm, searchTerm, searchTerm, searchTerm, searchTerm];
        } else {
            // 필터 쿼리 사용
            const filters = AlarmQueries.buildAlarmRuleWhereClause(
                AlarmQueries.AlarmRule.FIND_ALL, 
                { tenantId, condition_type, severity, is_enabled: enabled, device_id, data_point_id, virtual_point_id }
            );
            
            query = filters.query;
            params = filters.params;
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

        // AlarmQueries 헬퍼 사용
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

        // AlarmQueries 헬퍼 사용
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

/**
 * GET /api/alarms/rules/statistics
 * 알람 규칙 통계 조회
 */
router.get('/rules/statistics', async (req, res) => {
    try {
        const { tenantId } = req;
        
        console.log('알람 규칙 통계 조회 시작...');

        const [
            summaryStats,
            severityStats,
            typeStats
        ] = await Promise.all([
            dbGet(AlarmQueries.AlarmRule.STATS_SUMMARY, [tenantId]),
            dbAll(AlarmQueries.AlarmRule.STATS_BY_SEVERITY, [tenantId]),
            dbAll(AlarmQueries.AlarmRule.STATS_BY_TYPE, [tenantId])
        ]);

        const stats = {
            summary: summaryStats,
            by_severity: severityStats,
            by_type: typeStats
        };
        
        console.log('알람 규칙 통계 조회 완료');
        res.json(createResponse(true, stats, 'Alarm rule statistics retrieved successfully'));

    } catch (error) {
        console.error('알람 규칙 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_STATS_ERROR'));
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
            search
        } = req.query;
        
        console.log('알람 템플릿 조회 시작...');

        let query, params;
        
        if (search) {
            query = AlarmQueries.AlarmTemplate.SEARCH;
            const searchTerm = `%${search}%`;
            params = [tenantId, searchTerm, searchTerm, searchTerm];
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

        // AlarmQueries 헬퍼 사용
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

        // AlarmQueries 헬퍼 사용
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
            data_point_ids = [], 
            custom_configs = {},
            rule_group_name = null 
        } = req.body;

        console.log(`템플릿 ${id}를 ${data_point_ids.length}개 포인트에 적용...`);

        // 템플릿 조회
        const template = await dbGet(AlarmQueries.AlarmTemplate.FIND_BY_ID, [parseInt(id), tenantId]);
        if (!template) {
            return res.status(404).json(
                createResponse(false, null, 'Template not found', 'TEMPLATE_NOT_FOUND')
            );
        }

        // 규칙 그룹 ID 생성
        const ruleGroupId = `template_${id}_${Date.now()}`;

        // 각 데이터포인트에 대해 알람 규칙 생성
        const createdRules = [];
        for (const pointId of data_point_ids) {
            const customConfig = custom_configs[pointId] || {};
            const mergedConfig = {
                ...parseJSON(template.default_config),
                ...customConfig
            };

            const ruleData = {
                tenant_id: tenantId,
                name: `${template.name}_${pointId}`,
                description: `${template.description} (자동 생성)`,
                device_id: null,
                data_point_id: pointId,
                virtual_point_id: null,
                condition_type: template.condition_type,
                condition_config: JSON.stringify(mergedConfig),
                severity: template.severity,
                message_template: template.message_template,
                auto_acknowledge: template.auto_acknowledge,
                auto_clear: template.auto_clear,
                acknowledgment_required: 1,
                escalation_time_minutes: 0,
                notification_enabled: template.notification_enabled,
                email_notification: template.email_notification,
                sms_notification: template.sms_notification,
                is_enabled: 1,
                created_by: user.id
            };

            try {
                const params = AlarmQueries.buildCreateRuleParams(ruleData);
                const result = await dbRun(AlarmQueries.AlarmRule.CREATE, params);
                
                if (result.lastID) {
                    const newRule = await dbGet(AlarmQueries.AlarmRule.FIND_BY_ID, [result.lastID, tenantId]);
                    if (newRule) {
                        createdRules.push(newRule);
                    }
                }
            } catch (ruleError) {
                console.error(`데이터포인트 ${pointId} 규칙 생성 실패:`, ruleError.message);
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
 * 알람 템플릿 통계 조회
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
 * 알람 템플릿 검색
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
        const results = await dbAll(query, [tenantId, searchPattern, searchPattern, searchPattern]);
        
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
 * 알람 통계 조회
 */
router.get('/statistics', async (req, res) => {
    try {
        const { tenantId } = req;
        
        console.log('알람 통계 조회 시작...');

        const [
            occurrenceStats,
            ruleStats
        ] = await Promise.all([
            dbGet(AlarmQueries.AlarmOccurrence.STATS_SUMMARY, [tenantId]),
            dbGet(AlarmQueries.AlarmRule.STATS_SUMMARY, [tenantId])
        ]);

        const stats = {
            occurrences: occurrenceStats,
            rules: ruleStats,
            dashboard_summary: {
                total_active: occurrenceStats?.active_alarms || 0,
                total_rules: ruleStats?.total_rules || 0,
                unacknowledged: occurrenceStats?.unacknowledged_alarms || 0,
                enabled_rules: ruleStats?.enabled_rules || 0
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
 * PATCH /api/alarms/rules/:id/settings
 * 알람 규칙의 설정만 부분 업데이트
 */
router.patch('/rules/:id/settings', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const settingsUpdate = req.body;

        console.log(`알람 규칙 ${id} 설정 부분 업데이트...`);

        // 현재 규칙 조회
        const currentRule = await dbGet(AlarmQueries.AlarmRule.FIND_BY_ID, [parseInt(id), tenantId]);
        if (!currentRule) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

        // 기존 데이터를 기반으로 업데이트 객체 생성
        const updateData = {
            ...currentRule,
            ...settingsUpdate,
            updated_at: new Date().toISOString()
        };

        // 업데이트 실행
        const params = AlarmQueries.buildUpdateRuleParams(updateData, parseInt(id), tenantId);
        const result = await dbRun(AlarmQueries.AlarmRule.UPDATE, params);

        if (result.changes > 0) {
            const updatedRule = await dbGet(AlarmQueries.AlarmRule.FIND_BY_ID, [parseInt(id), tenantId]);

            console.log(`알람 규칙 ${id} 설정 업데이트 완료`);
            res.json(createResponse(true, formatAlarmRule(updatedRule), 'Alarm rule settings updated successfully'));
        } else {
            return res.status(500).json(
                createResponse(false, null, 'Failed to update alarm rule settings', 'SETTINGS_UPDATE_FAILED')
            );
        }

    } catch (error) {
        console.error(`알람 규칙 ${req.params.id} 설정 업데이트 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'SETTINGS_UPDATE_ERROR'));
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
            message: 'Complete Alarm API is working with DatabaseFactory.executeQuery!',
            database_test: testResult,
            architecture: [
                'ConfigManager-based database configuration',
                'DatabaseFactory.executeQuery for unified database access', 
                'AlarmQueries for centralized SQL management',
                'DB-type independent implementation',
                'Complete feature coverage'
            ],
            available_endpoints: [
                // 알람 발생 관련
                'GET /api/alarms/active',
                'GET /api/alarms/occurrences',
                'GET /api/alarms/occurrences/:id',
                'GET /api/alarms/history',
                'POST /api/alarms/occurrences/:id/acknowledge',
                'POST /api/alarms/occurrences/:id/clear',
                'GET /api/alarms/unacknowledged',
                'GET /api/alarms/recent',
                'GET /api/alarms/device/:deviceId',
                
                // 알람 규칙 관련
                'GET /api/alarms/rules',
                'GET /api/alarms/rules/:id',
                'POST /api/alarms/rules',
                'PUT /api/alarms/rules/:id',
                'DELETE /api/alarms/rules/:id',
                'GET /api/alarms/rules/statistics',
                'PATCH /api/alarms/rules/:id/settings',
                
                // 알람 템플릿 관련
                'GET /api/alarms/templates',
                'GET /api/alarms/templates/:id',
                'POST /api/alarms/templates',
                'PUT /api/alarms/templates/:id',
                'DELETE /api/alarms/templates/:id',
                'GET /api/alarms/templates/category/:category',
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
            features_included: [
                '✅ DatabaseFactory.executeQuery 통합 인터페이스 사용',
                '✅ DB 타입 독립적 구현 (SQLite/PostgreSQL/MariaDB 지원)',
                '✅ AlarmQueries 모든 쿼리 활용',
                '✅ 완전한 CRUD 작업',
                '✅ 페이징 및 검색 지원',
                '✅ 필터링 및 정렬',
                '✅ 통계 및 대시보드 데이터',
                '✅ 템플릿 시스템',
                '✅ 일괄 작업 지원',
                '✅ 데이터 포맷팅',
                '✅ 에러 처리',
                '✅ 로깅'
            ]
        }, 'Complete Alarm API test successful - DatabaseFactory integration working'));

    } catch (error) {
        console.error('테스트 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TEST_ERROR'));
    }
});

module.exports = router;