// ============================================================================
// backend/routes/alarms.js
// 완전한 알람 관리 API - Repository 패턴 활용 (완성본)
// ============================================================================

const express = require('express');
const router = express.Router();

// Repository imports (실제 구현한 것들 사용)
const AlarmRuleRepository = require('../lib/database/repositories/AlarmRuleRepository');
const AlarmOccurrenceRepository = require('../lib/database/repositories/AlarmOccurrenceRepository');
const AlarmTemplateRepository = require('../lib/database/repositories/AlarmTemplateRepository');

// Repository 인스턴스 생성
let alarmRuleRepo = null;
let alarmOccurrenceRepo = null;
let alarmTemplateRepo = null;

function getAlarmRuleRepo() {
    if (!alarmRuleRepo) {
        alarmRuleRepo = new AlarmRuleRepository();
        console.log("✅ AlarmRuleRepository 초기화 완료");
    }
    return alarmRuleRepo;
}

function getAlarmOccurrenceRepo() {
    if (!alarmOccurrenceRepo) {
        alarmOccurrenceRepo = new AlarmOccurrenceRepository();
        console.log("✅ AlarmOccurrenceRepository 초기화 완료");
    }
    return alarmOccurrenceRepo;
}

function getAlarmTemplateRepo() {
    if (!alarmTemplateRepo) {
        alarmTemplateRepo = new AlarmTemplateRepository();
        console.log("✅ AlarmTemplateRepository 초기화 완료");
    }
    return alarmTemplateRepo;
}

// ============================================================================
// 🛡️ 미들웨어 및 헬퍼 함수들
// ============================================================================

/**
 * 표준 응답 생성
 */
function createResponse(success, data, message, error_code) {
    return {
        success,
        data,
        message: message || (success ? 'Success' : 'Error'),
        error_code: error_code || null,
        timestamp: new Date().toISOString()
    };
}

/**
 * 인증 미들웨어 (개발용)
 */
const authenticateToken = (req, res, next) => {
    // 개발 단계에서는 기본 사용자 설정
    req.user = {
        id: 1,
        username: 'admin',
        tenant_id: 1,
        role: 'admin'
    };
    next();
};

/**
 * 테넌트 격리 미들웨어
 */
const tenantIsolation = (req, res, next) => {
    req.tenantId = req.user.tenant_id;
    next();
};

// 글로벌 미들웨어 적용
router.use(authenticateToken);
router.use(tenantIsolation);

// ============================================================================
// 🚨 알람 발생 (Alarm Occurrences) API
// ============================================================================

/**
 * GET /api/alarms/active
 * 활성 알람 목록 조회 (ActiveAlarms.tsx용) - 수정됨
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
        
        console.log('🔍 활성 알람 조회 시작...');

        // 🔥 findAll 대신 findActive 직접 호출
        const activeAlarms = await getAlarmOccurrenceRepo().findActive(tenantId);
        
        // 클라이언트 측에서 필터링 (임시)
        let filteredAlarms = activeAlarms;
        
        if (severity) {
            filteredAlarms = filteredAlarms.filter(alarm => 
                alarm.severity && alarm.severity.toLowerCase() === severity.toLowerCase()
            );
        }
        
        if (device_id) {
            filteredAlarms = filteredAlarms.filter(alarm => 
                alarm.device_id === device_id || alarm.device_id === parseInt(device_id)
            );
        }
        
        // 페이징 처리
        const startIndex = (parseInt(page) - 1) * parseInt(limit);
        const endIndex = startIndex + parseInt(limit);
        const paginatedAlarms = filteredAlarms.slice(startIndex, endIndex);
        
        const result = {
            items: paginatedAlarms,
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: filteredAlarms.length,
                totalPages: Math.ceil(filteredAlarms.length / parseInt(limit))
            }
        };
        
        console.log(`✅ 활성 알람 ${paginatedAlarms.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Active alarms retrieved successfully'));

    } catch (error) {
        console.error('❌ 활성 알람 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ACTIVE_ALARMS_ERROR'));
    }
});

/**
 * GET /api/alarms/occurrences
 * 모든 알람 발생 조회 (페이징 지원) - 누락된 핵심 엔드포인트!
 */
router.get('/occurrences', async (req, res) => {
    try {
        const { tenantId } = req;
        const {
            page = 1,
            limit = 50,
            state,
            severity,
            ruleId,
            deviceId
        } = req.query;
        
        console.log('🔍 알람 발생 목록 조회 시작...');

        const options = {
            tenantId: parseInt(tenantId),
            page: parseInt(page),
            limit: parseInt(limit),
            state,
            severity,
            ruleId: ruleId ? parseInt(ruleId) : null,
            deviceId
        };

        const result = await getAlarmOccurrenceRepo().findAll(options);
        
        console.log(`✅ 알람 발생 ${result.items.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Alarm occurrences retrieved successfully'));

    } catch (error) {
        console.error('❌ 알람 발생 조회 실패:', error.message);
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
        
        console.log(`🔍 알람 발생 ID ${id} 조회 시작...`);

        const alarmOccurrence = await getAlarmOccurrenceRepo().findById(parseInt(id), tenantId);
        
        if (!alarmOccurrence) {
            return res.status(404).json(createResponse(false, null, 'Alarm occurrence not found', 'ALARM_NOT_FOUND'));
        }

        console.log(`✅ 알람 발생 ID ${id} 조회 완료`);
        res.json(createResponse(true, alarmOccurrence, 'Alarm occurrence retrieved successfully'));

    } catch (error) {
        console.error(`❌ 알람 발생 ID ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_OCCURRENCE_ERROR'));
    }
});

/**
 * GET /api/alarms/history
 * 알람 이력 조회 (AlarmHistory.tsx용)
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
        
        console.log('📜 알람 이력 조회 시작...');

        const options = {
            tenantId,
            severity,
            deviceId: device_id,
            state: state || undefined, // 모든 상태 포함
            page: parseInt(page),
            limit: parseInt(limit)
        };

        const result = await getAlarmOccurrenceRepo().findAll(options);
        
        console.log(`✅ 알람 이력 ${result.items.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Alarm history retrieved successfully'));

    } catch (error) {
        console.error('❌ 알람 이력 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_HISTORY_ERROR'));
    }
});

/**
 * POST /api/alarms/:id/acknowledge
 * 알람 확인 처리
 */
router.post('/:id/acknowledge', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment = '' } = req.body;
        const { user, tenantId } = req;
        
        console.log(`✅ 알람 ${id} 확인 처리 시작...`);

        const result = await getAlarmOccurrenceRepo().acknowledge(
            parseInt(id), 
            user.id, 
            comment, 
            tenantId
        );

        if (!result) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm not found or update failed', 'ALARM_NOT_FOUND')
            );
        }

        // 실시간 이벤트 발송 (WebSocket이 있다면)
        if (req.app.get('io')) {
            req.app.get('io').to(`tenant:${tenantId}`).emit('alarm_acknowledged', {
                type: 'alarm_acknowledged',
                alarm_id: id,
                acknowledged_by: user.username,
                acknowledged_time: new Date().toISOString(),
                comment
            });
        }

        console.log(`✅ 알람 ${id} 확인 처리 완료`);
        res.json(createResponse(true, result, 'Alarm acknowledged successfully'));

    } catch (error) {
        console.error(`❌ 알람 ${req.params.id} 확인 처리 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_ACKNOWLEDGE_ERROR'));
    }
});

/**
 * POST /api/alarms/occurrences/:id/acknowledge
 * 알람 확인 처리 (occurrences 경로)
 */
router.post('/occurrences/:id/acknowledge', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment = '' } = req.body;
        const { user, tenantId } = req;
        
        console.log(`✅ 알람 발생 ${id} 확인 처리 시작...`);

        const result = await getAlarmOccurrenceRepo().acknowledge(
            parseInt(id), 
            user.id, 
            comment, 
            tenantId
        );

        if (!result) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm occurrence not found or already acknowledged', 'ALARM_NOT_FOUND')
            );
        }

        console.log(`✅ 알람 발생 ${id} 확인 처리 완료`);
        res.json(createResponse(true, result, 'Alarm occurrence acknowledged successfully'));

    } catch (error) {
        console.error(`❌ 알람 발생 ${req.params.id} 확인 처리 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_ACKNOWLEDGE_ERROR'));
    }
});

/**
 * POST /api/alarms/:id/clear
 * 알람 해제 처리
 */
router.post('/:id/clear', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment = '', clearedValue = '' } = req.body;
        const { user, tenantId } = req;
        
        console.log(`🗑️ 알람 ${id} 해제 처리 시작...`);

        const result = await getAlarmOccurrenceRepo().clear(
            parseInt(id), 
            clearedValue, 
            comment, 
            tenantId
        );

        if (!result) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm not found or update failed', 'ALARM_NOT_FOUND')
            );
        }

        // 실시간 이벤트 발송 (WebSocket이 있다면)
        if (req.app.get('io')) {
            req.app.get('io').to(`tenant:${tenantId}`).emit('alarm_cleared', {
                type: 'alarm_cleared',
                alarm_id: id,
                cleared_by: user.username,
                cleared_at: new Date().toISOString(),
                comment
            });
        }

        console.log(`✅ 알람 ${id} 해제 처리 완료`);
        res.json(createResponse(true, result, 'Alarm cleared successfully'));

    } catch (error) {
        console.error(`❌ 알람 ${req.params.id} 해제 처리 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_CLEAR_ERROR'));
    }
});

/**
 * POST /api/alarms/occurrences/:id/clear
 * 알람 해제 처리 (occurrences 경로)
 */
router.post('/occurrences/:id/clear', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment = '', clearedValue = '' } = req.body;
        const { tenantId } = req;
        
        console.log(`🗑️ 알람 발생 ${id} 해제 처리 시작...`);

        const result = await getAlarmOccurrenceRepo().clear(
            parseInt(id), 
            clearedValue, 
            comment, 
            tenantId
        );

        if (!result) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm occurrence not found', 'ALARM_NOT_FOUND')
            );
        }

        console.log(`✅ 알람 발생 ${id} 해제 처리 완료`);
        res.json(createResponse(true, result, 'Alarm occurrence cleared successfully'));

    } catch (error) {
        console.error(`❌ 알람 발생 ${req.params.id} 해제 처리 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_CLEAR_ERROR'));
    }
});

/**
 * GET /api/alarms/statistics
 * 알람 통계 조회 (대시보드용)
 */
router.get('/statistics', async (req, res) => {
    try {
        const { tenantId } = req;
        
        console.log('📊 알람 통계 조회 시작...');

        const stats = await getAlarmOccurrenceRepo().getStatsByTenant(tenantId);
        
        console.log('✅ 알람 통계 조회 완료');
        res.json(createResponse(true, stats, 'Alarm statistics retrieved successfully'));

    } catch (error) {
        console.error('❌ 알람 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_STATS_ERROR'));
    }
});

// ============================================================================
// 🔧 알람 규칙 (Alarm Rules) API
// ============================================================================

/**
 * GET /api/alarms/rules
 * 알람 규칙 목록 조회 (AlarmRules.tsx용)
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
            target_type
        } = req.query;
        
        console.log('🔍 알람 규칙 조회 시작...');

        const options = {
            tenantId,
            enabled: enabled !== undefined ? enabled === 'true' : undefined,
            alarmType: alarm_type,
            severity,
            targetType: target_type,
            page: parseInt(page),
            limit: parseInt(limit)
        };

        const result = await getAlarmRuleRepo().findAll(options);
        
        console.log(`✅ 알람 규칙 ${result.items.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Alarm rules retrieved successfully'));

    } catch (error) {
        console.error('❌ 알람 규칙 조회 실패:', error.message);
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
        
        console.log(`🔍 알람 규칙 ID ${id} 조회...`);

        const alarmRule = await getAlarmRuleRepo().findById(parseInt(id), tenantId);

        if (!alarmRule) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

        console.log(`✅ 알람 규칙 ID ${id} 조회 완료`);
        res.json(createResponse(true, alarmRule, 'Alarm rule retrieved successfully'));

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 조회 실패:`, error.message);
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
            created_by: user.id
        };

        console.log('🔧 새 알람 규칙 생성...');

        const newAlarmRule = await getAlarmRuleRepo().create(alarmRuleData, tenantId);

        console.log(`✅ 새 알람 규칙 생성 완료: ID ${newAlarmRule.id}`);
        res.status(201).json(createResponse(true, newAlarmRule, 'Alarm rule created successfully'));

    } catch (error) {
        console.error('❌ 알람 규칙 생성 실패:', error.message);
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

        console.log(`🔧 알람 규칙 ${id} 수정...`);

        const updatedAlarmRule = await getAlarmRuleRepo().update(parseInt(id), updateData, tenantId);

        if (!updatedAlarmRule) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found or update failed', 'ALARM_RULE_UPDATE_FAILED')
            );
        }

        console.log(`✅ 알람 규칙 ID ${id} 수정 완료`);
        res.json(createResponse(true, updatedAlarmRule, 'Alarm rule updated successfully'));

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 수정 실패:`, error.message);
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

        console.log(`🗑️ 알람 규칙 ${id} 삭제...`);

        const deleted = await getAlarmRuleRepo().deleteById(parseInt(id), tenantId);

        if (!deleted) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found or delete failed', 'ALARM_RULE_DELETE_FAILED')
            );
        }

        console.log(`✅ 알람 규칙 ID ${id} 삭제 완료`);
        res.json(createResponse(true, { deleted: true }, 'Alarm rule deleted successfully'));

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 삭제 실패:`, error.message);
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
        
        console.log('📊 알람 규칙 통계 조회 시작...');

        const stats = await getAlarmRuleRepo().getStatsByTenant(tenantId);
        
        console.log('✅ 알람 규칙 통계 조회 완료');
        res.json(createResponse(true, stats, 'Alarm rule statistics retrieved successfully'));

    } catch (error) {
        console.error('❌ 알람 규칙 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_STATS_ERROR'));
    }
});

// ============================================================================
// 🔧 특화 API 엔드포인트들
// ============================================================================

/**
 * GET /api/alarms/unacknowledged
 * 미확인 알람만 조회
 */
router.get('/unacknowledged', async (req, res) => {
    try {
        const { tenantId } = req;
        
        console.log('🔍 미확인 알람 조회 시작...');

        const unacknowledgedAlarms = await getAlarmOccurrenceRepo().findUnacknowledged(tenantId);
        
        console.log(`✅ 미확인 알람 ${unacknowledgedAlarms.length}개 조회 완료`);
        res.json(createResponse(true, unacknowledgedAlarms, 'Unacknowledged alarms retrieved successfully'));

    } catch (error) {
        console.error('❌ 미확인 알람 조회 실패:', error.message);
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
        
        console.log(`🔍 디바이스 ${deviceId} 알람 조회 시작...`);

        const deviceAlarms = await getAlarmOccurrenceRepo().findByDevice(deviceId, tenantId);
        
        console.log(`✅ 디바이스 ${deviceId} 알람 ${deviceAlarms.length}개 조회 완료`);
        res.json(createResponse(true, deviceAlarms, 'Device alarms retrieved successfully'));

    } catch (error) {
        console.error(`❌ 디바이스 ${req.params.deviceId} 알람 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_ALARMS_ERROR'));
    }
});

/**
 * GET /api/alarms/test
 * 알람 API 테스트 엔드포인트
 */
router.get('/test', (req, res) => {
    res.json(createResponse(true, { 
        message: 'Alarm API is working!',
        repositories: {
            alarm_rules: 'AlarmRuleRepository ready',
            alarm_occurrences: 'AlarmOccurrenceRepository ready'
        },
        available_endpoints: [
            'GET /api/alarms/active',
            'GET /api/alarms/occurrences',
            'GET /api/alarms/occurrences/:id',
            'POST /api/alarms/occurrences/:id/acknowledge',
            'POST /api/alarms/occurrences/:id/clear',
            'GET /api/alarms/history',
            'POST /api/alarms/:id/acknowledge',
            'POST /api/alarms/:id/clear',
            'GET /api/alarms/statistics',
            'GET /api/alarms/rules',
            'GET /api/alarms/rules/:id',
            'POST /api/alarms/rules',
            'PUT /api/alarms/rules/:id',
            'DELETE /api/alarms/rules/:id',
            'GET /api/alarms/rules/statistics',
            'GET /api/alarms/unacknowledged',
            'GET /api/alarms/device/:deviceId',
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
            'GET /api/alarms/test'
        ]
    }, 'Test successful'));
});

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
        
        console.log('🎯 알람 템플릿 조회 시작...');

        const options = {
            tenantId,
            category,
            is_system_template: is_system_template !== undefined ? is_system_template === 'true' : undefined,
            search,
            page: parseInt(page),
            limit: parseInt(limit)
        };

        const result = await getAlarmTemplateRepo().findAll(options);
        
        console.log(`✅ 알람 템플릿 ${result.items.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Alarm templates retrieved successfully'));

    } catch (error) {
        console.error('❌ 알람 템플릿 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_TEMPLATES_ERROR'));
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
            created_by: user.id
        };

        console.log('🎯 새 알람 템플릿 생성...');

        const newTemplate = await getAlarmTemplateRepo().create(templateData, tenantId);

        console.log(`✅ 새 알람 템플릿 생성 완료: ID ${newTemplate.id}`);
        res.status(201).json(createResponse(true, newTemplate, 'Alarm template created successfully'));

    } catch (error) {
        console.error('❌ 알람 템플릿 생성 실패:', error.message);
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

        console.log(`🎯 알람 템플릿 ${id} 수정...`);

        const updatedTemplate = await getAlarmTemplateRepo().update(parseInt(id), updateData, tenantId);

        if (!updatedTemplate) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm template not found or update failed', 'ALARM_TEMPLATE_UPDATE_FAILED')
            );
        }

        console.log(`✅ 알람 템플릿 ID ${id} 수정 완료`);
        res.json(createResponse(true, updatedTemplate, 'Alarm template updated successfully'));

    } catch (error) {
        console.error(`❌ 알람 템플릿 ${req.params.id} 수정 실패:`, error.message);
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

        console.log(`🎯 알람 템플릿 ${id} 삭제...`);

        const deleted = await getAlarmTemplateRepo().delete(parseInt(id), tenantId);

        if (!deleted) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm template not found or delete failed', 'ALARM_TEMPLATE_DELETE_FAILED')
            );
        }

        console.log(`✅ 알람 템플릿 ID ${id} 삭제 완료`);
        res.json(createResponse(true, { deleted: true }, 'Alarm template deleted successfully'));

    } catch (error) {
        console.error(`❌ 알람 템플릿 ${req.params.id} 삭제 실패:`, error.message);
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
        
        console.log(`🎯 카테고리 ${category} 템플릿 조회...`);

        const templates = await getAlarmTemplateRepo().findByCategory(category, tenantId);
        
        console.log(`✅ 카테고리 ${category} 템플릿 ${templates.length}개 조회 완료`);
        res.json(createResponse(true, templates, 'Category templates retrieved successfully'));

    } catch (error) {
        console.error(`❌ 카테고리 ${req.params.category} 템플릿 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CATEGORY_TEMPLATES_ERROR'));
    }
});

/**
 * GET /api/alarms/templates/system
 * 시스템 기본 템플릿 조회
 */
router.get('/templates/system', async (req, res) => {
    try {
        console.log('🎯 시스템 템플릿 조회...');

        const templates = await getAlarmTemplateRepo().findSystemTemplates();
        
        console.log(`✅ 시스템 템플릿 ${templates.length}개 조회 완료`);
        res.json(createResponse(true, templates, 'System templates retrieved successfully'));

    } catch (error) {
        console.error('❌ 시스템 템플릿 조회 실패:', error.message);
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
        
        console.log(`🎯 데이터 타입 ${dataType} 적용 가능 템플릿 조회...`);

        const templates = await getAlarmTemplateRepo().findByDataType(dataType, tenantId);
        
        console.log(`✅ 데이터 타입 ${dataType} 템플릿 ${templates.length}개 조회 완료`);
        res.json(createResponse(true, templates, 'Data type templates retrieved successfully'));

    } catch (error) {
        console.error(`❌ 데이터 타입 ${req.params.dataType} 템플릿 조회 실패:`, error.message);
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

        console.log(`🎯 템플릿 ${id}를 ${data_point_ids.length}개 포인트에 적용...`);

        // 템플릿 조회
        const template = await getAlarmTemplateRepo().findById(parseInt(id), tenantId);
        if (!template) {
            return res.status(404).json(
                createResponse(false, null, 'Template not found', 'TEMPLATE_NOT_FOUND')
            );
        }

        // 규칙 그룹 ID 생성 (UUID)
        const { v4: uuidv4 } = require('uuid');
        const ruleGroupId = uuidv4();

        // 각 데이터포인트에 대해 알람 규칙 생성
        const createdRules = [];
        for (const pointId of data_point_ids) {
            const customConfig = custom_configs[pointId] || {};
            const mergedConfig = {
                ...template.default_config,
                ...customConfig
            };

            const ruleData = {
                tenant_id: tenantId,
                name: `${template.name}_${pointId}`,
                description: `${template.description} (자동 생성)`,
                target_type: 'data_point',
                target_id: pointId,
                alarm_type: template.condition_type === 'threshold' ? 'analog' : 
                           template.condition_type === 'digital' ? 'digital' : 'script',
                severity: template.severity,
                high_limit: mergedConfig.threshold || mergedConfig.high_limit || null,
                low_limit: mergedConfig.low_threshold || mergedConfig.low_limit || null,
                deadband: mergedConfig.hysteresis || mergedConfig.deadband || 0,
                message_template: template.message_template,
                notification_enabled: template.notification_enabled,
                email_notification: template.email_notification,
                sms_notification: template.sms_notification,
                auto_acknowledge: template.auto_acknowledge,
                auto_clear: template.auto_clear,
                template_id: template.id,
                rule_group: ruleGroupId,
                created_by_template: 1,
                created_by: user.id
            };

            try {
                const newRule = await getAlarmRuleRepo().create(ruleData, tenantId);
                if (newRule) {
                    createdRules.push(newRule);
                }
            } catch (ruleError) {
                console.error(`데이터포인트 ${pointId} 규칙 생성 실패:`, ruleError.message);
            }
        }

        // 템플릿 사용량 증가
        await getAlarmTemplateRepo().incrementUsage(template.id, createdRules.length);

        console.log(`✅ 템플릿 적용 완료: ${createdRules.length}개 규칙 생성`);
        res.json(createResponse(true, {
            template_id: template.id,
            template_name: template.name,
            rule_group_id: ruleGroupId,
            rules_created: createdRules.length,
            created_rules: createdRules
        }, 'Template applied successfully'));

    } catch (error) {
        console.error(`❌ 템플릿 ${req.params.id} 적용 실패:`, error.message);
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
        
        console.log(`🎯 템플릿 ${id}로 생성된 규칙들 조회...`);

        const appliedRules = await getAlarmTemplateRepo().findAppliedRules(parseInt(id), tenantId);
        
        console.log(`✅ 템플릿 ${id}로 생성된 규칙 ${appliedRules.length}개 조회 완료`);
        res.json(createResponse(true, appliedRules, 'Applied rules retrieved successfully'));

    } catch (error) {
        console.error(`❌ 템플릿 ${req.params.id} 적용 규칙 조회 실패:`, error.message);
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
        
        console.log('🎯 알람 템플릿 통계 조회 시작...');

        const stats = await getAlarmTemplateRepo().getStatistics(tenantId);
        
        console.log('✅ 알람 템플릿 통계 조회 완료');
        res.json(createResponse(true, stats, 'Template statistics retrieved successfully'));

    } catch (error) {
        console.error('❌ 알람 템플릿 통계 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'TEMPLATE_STATS_ERROR'));
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
        
        console.log(`🎯 알람 템플릿 ID ${id} 조회...`);

        const template = await getAlarmTemplateRepo().findById(parseInt(id), tenantId);

        if (!template) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm template not found', 'ALARM_TEMPLATE_NOT_FOUND')
            );
        }

        console.log(`✅ 알람 템플릿 ID ${id} 조회 완료`);
        res.json(createResponse(true, template, 'Alarm template retrieved successfully'));

    } catch (error) {
        console.error(`❌ 알람 템플릿 ${req.params.id} 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_TEMPLATE_DETAIL_ERROR'));
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

        console.log(`🎯 알람 템플릿 검색: "${searchTerm}"`);

        const templates = await getAlarmTemplateRepo().search(searchTerm, tenantId, parseInt(limit));
        
        console.log(`✅ 검색 결과: ${templates.length}개 템플릿`);
        res.json(createResponse(true, templates, 'Template search completed successfully'));

    } catch (error) {
        console.error(`❌ 템플릿 검색 실패:`, error.message);
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
        
        console.log('🎯 인기 템플릿 조회...');

        const templates = await getAlarmTemplateRepo().findMostUsed(tenantId, parseInt(limit));
        
        console.log(`✅ 인기 템플릿 ${templates.length}개 조회 완료`);
        res.json(createResponse(true, templates, 'Most used templates retrieved successfully'));

    } catch (error) {
        console.error('❌ 인기 템플릿 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'MOST_USED_TEMPLATES_ERROR'));
    }
});

/**
 * PATCH /api/alarms/rules/:id/settings
 * 알람 규칙의 설정만 부분 업데이트 (AlarmSettings.tsx 전용)
 * 
 * Body 예시:
 * {
 *   "highLimit": 85,
 *   "deadband": 2.5,
 *   "emailEnabled": true,
 *   "emailRecipients": ["admin@company.com"]
 * }
 */
router.patch('/rules/:id/settings', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const settingsUpdate = req.body;

        console.log(`🔧 알람 규칙 ${id} 설정 부분 업데이트...`);

        // 현재 규칙 조회
        const currentRule = await getAlarmRuleRepo().findById(parseInt(id), tenantId);
        if (!currentRule) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

        // 설정 매핑 (프론트엔드 필드명 -> DB 필드명)
        const fieldMapping = {
            // 임계값 설정
            'highHighLimit': 'high_high_limit',
            'highLimit': 'high_limit', 
            'lowLimit': 'low_limit',
            'lowLowLimit': 'low_low_limit',
            'deadband': 'deadband',
            'targetValue': 'target_value',
            'tolerance': 'tolerance',
            'timeWindow': 'time_window',
            
            // 우선순위 및 동작
            'priority': 'severity',
            'severity': 'severity_level',
            'autoAcknowledge': 'auto_acknowledge',
            'autoReset': 'auto_clear',
            'suppressDuration': 'suppression_duration',
            'maxOccurrences': 'max_occurrences', 
            'escalationTime': 'escalation_time_minutes',
            
            // 알림 설정
            'emailEnabled': 'email_notification',
            'smsEnabled': 'sms_notification',
            'soundEnabled': 'sound_notification',
            'popupEnabled': 'popup_notification',
            'webhookEnabled': 'webhook_notification',
            'webhookUrl': 'webhook_url',
            
            // 메시지 설정
            'messageTemplate': 'message_template',
            'emailTemplate': 'email_template',
            
            // 상태
            'isEnabled': 'is_enabled'
        };

        // 기존 규칙 데이터를 기반으로 업데이트 객체 생성
        const updateData = {
            // 필수 필드들 (기존 값 유지)
            name: currentRule.name,
            description: currentRule.description,
            target_type: currentRule.target_type,
            target_id: currentRule.target_id,
            alarm_type: currentRule.alarm_type,
            severity: currentRule.severity,
            
            // 기존 모든 필드 복사
            target_group: currentRule.target_group,
            high_high_limit: currentRule.high_high_limit,
            high_limit: currentRule.high_limit,
            low_limit: currentRule.low_limit,
            low_low_limit: currentRule.low_low_limit,
            deadband: currentRule.deadband,
            rate_of_change: currentRule.rate_of_change,
            trigger_condition: currentRule.trigger_condition,
            condition_script: currentRule.condition_script,
            message_script: currentRule.message_script,
            message_config: currentRule.message_config,
            message_template: currentRule.message_template,
            priority: currentRule.priority,
            auto_acknowledge: currentRule.auto_acknowledge,
            acknowledge_timeout_min: currentRule.acknowledge_timeout_min,
            auto_clear: currentRule.auto_clear,
            suppression_rules: currentRule.suppression_rules,
            notification_enabled: currentRule.notification_enabled,
            notification_delay_sec: currentRule.notification_delay_sec,
            notification_repeat_interval_min: currentRule.notification_repeat_interval_min,
            notification_channels: currentRule.notification_channels,
            notification_recipients: currentRule.notification_recipients,
            is_enabled: currentRule.is_enabled,
            is_latched: currentRule.is_latched,
            template_id: currentRule.template_id,
            rule_group: currentRule.rule_group,
            created_by_template: currentRule.created_by_template,
            escalation_rules: currentRule.escalation_rules,
            
            // updated_at 필드 갱신
            updated_at: new Date().toISOString()
        };

        // 변경할 설정만 덮어쓰기
        Object.entries(settingsUpdate).forEach(([frontendKey, value]) => {
            const dbKey = fieldMapping[frontendKey] || frontendKey;
            
            // 특별 처리가 필요한 필드들
            if (frontendKey === 'emailRecipients' || frontendKey === 'smsRecipients') {
                // 배열을 JSON 문자열로 변환
                updateData[dbKey] = JSON.stringify(value);
            } else if (frontendKey === 'schedule') {
                // 스케줄 객체를 JSON으로 저장
                updateData['schedule_config'] = JSON.stringify(value);
            } else if (frontendKey === 'priority') {
                // priority 값을 severity로 매핑
                const priorityMap = { 'low': 'low', 'medium': 'medium', 'high': 'high', 'critical': 'critical' };
                updateData['severity'] = priorityMap[value] || value;
            } else {
                // 직접 필드 업데이트
                updateData[dbKey] = value;
            }
        });

        // 데이터베이스 업데이트
        const updatedRule = await getAlarmRuleRepo().update(parseInt(id), updateData, tenantId);

        if (!updatedRule) {
            return res.status(500).json(
                createResponse(false, null, 'Failed to update alarm rule settings', 'SETTINGS_UPDATE_FAILED')
            );
        }

        console.log(`✅ 알람 규칙 ${id} 설정 업데이트 완료`);
        res.json(createResponse(true, updatedRule, 'Alarm rule settings updated successfully'));

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 설정 업데이트 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'SETTINGS_UPDATE_ERROR'));
    }
});

/**
 * PATCH /api/alarms/rules/bulk-update
 * 여러 알람 규칙의 설정을 일괄 업데이트 (AlarmSettings.tsx 일괄 수정용)
 * 
 * Body 예시:
 * {
 *   "rule_ids": [1, 2, 3, 4],
 *   "settings": {
 *     "emailEnabled": true,
 *     "suppressDuration": 300,
 *     "autoAcknowledge": false
 *   }
 * }
 */
router.patch('/rules/bulk-update', async (req, res) => {
    try {
        const { tenantId } = req;
        const { rule_ids = [], settings = {} } = req.body;

        if (!Array.isArray(rule_ids) || rule_ids.length === 0) {
            return res.status(400).json(
                createResponse(false, null, 'rule_ids array is required and must not be empty', 'INVALID_RULE_IDS')
            );
        }

        if (Object.keys(settings).length === 0) {
            return res.status(400).json(
                createResponse(false, null, 'settings object is required and must not be empty', 'INVALID_SETTINGS')
            );
        }

        console.log(`🔧 ${rule_ids.length}개 알람 규칙 일괄 설정 업데이트...`);

        // 설정 매핑 (개별 업데이트와 동일)
        const fieldMapping = {
            'highHighLimit': 'high_high_limit',
            'highLimit': 'high_limit', 
            'lowLimit': 'low_limit',
            'lowLowLimit': 'low_low_limit',
            'deadband': 'deadband',
            'priority': 'severity',
            'autoAcknowledge': 'auto_acknowledge',
            'autoReset': 'auto_clear',
            'suppressDuration': 'suppression_duration',
            'maxOccurrences': 'max_occurrences',
            'escalationTime': 'escalation_time_minutes',
            'emailEnabled': 'email_notification',
            'smsEnabled': 'sms_notification',
            'webhookEnabled': 'webhook_notification',
            'messageTemplate': 'message_template',
            'isEnabled': 'is_enabled'
        };

        // 일괄 업데이트 실행
        const updateResults = [];
        const failedUpdates = [];

        for (const ruleId of rule_ids) {
            try {
                // 1. 현재 규칙 조회
                const currentRule = await getAlarmRuleRepo().findById(parseInt(ruleId), tenantId);
                if (!currentRule) {
                    failedUpdates.push({ rule_id: ruleId, error: 'Rule not found' });
                    continue;
                }

                // 2. 기존 데이터를 기반으로 업데이트 객체 생성
                const updateData = {
                    // 필수 필드들 (기존 값 유지)
                    name: currentRule.name,
                    description: currentRule.description,
                    target_type: currentRule.target_type,
                    target_id: currentRule.target_id,
                    alarm_type: currentRule.alarm_type,
                    severity: currentRule.severity,
                    
                    // 모든 기존 필드 복사
                    target_group: currentRule.target_group,
                    high_high_limit: currentRule.high_high_limit,
                    high_limit: currentRule.high_limit,
                    low_limit: currentRule.low_limit,
                    low_low_limit: currentRule.low_low_limit,
                    deadband: currentRule.deadband,
                    rate_of_change: currentRule.rate_of_change,
                    trigger_condition: currentRule.trigger_condition,
                    condition_script: currentRule.condition_script,
                    message_script: currentRule.message_script,
                    message_config: currentRule.message_config,
                    message_template: currentRule.message_template,
                    priority: currentRule.priority,
                    auto_acknowledge: currentRule.auto_acknowledge,
                    acknowledge_timeout_min: currentRule.acknowledge_timeout_min,
                    auto_clear: currentRule.auto_clear,
                    suppression_rules: currentRule.suppression_rules,
                    notification_enabled: currentRule.notification_enabled,
                    notification_delay_sec: currentRule.notification_delay_sec,
                    notification_repeat_interval_min: currentRule.notification_repeat_interval_min,
                    notification_channels: currentRule.notification_channels,
                    notification_recipients: currentRule.notification_recipients,
                    is_enabled: currentRule.is_enabled,
                    is_latched: currentRule.is_latched,
                    template_id: currentRule.template_id,
                    rule_group: currentRule.rule_group,
                    created_by_template: currentRule.created_by_template,
                    escalation_rules: currentRule.escalation_rules,
                    
                    // updated_at 갱신
                    updated_at: new Date().toISOString()
                };

                // 3. 변경할 설정만 덮어쓰기
                Object.entries(settings).forEach(([frontendKey, value]) => {
                    const dbKey = fieldMapping[frontendKey] || frontendKey;
                    
                    if (frontendKey === 'emailRecipients' || frontendKey === 'smsRecipients') {
                        updateData[dbKey] = JSON.stringify(value);
                    } else if (frontendKey === 'priority') {
                        const priorityMap = { 'low': 'low', 'medium': 'medium', 'high': 'high', 'critical': 'critical' };
                        updateData['severity'] = priorityMap[value] || value;
                    } else {
                        updateData[dbKey] = value;
                    }
                });

                // 4. 업데이트 실행
                const updated = await getAlarmRuleRepo().update(parseInt(ruleId), updateData, tenantId);
                if (updated) {
                    updateResults.push({ rule_id: ruleId, success: true });
                } else {
                    failedUpdates.push({ rule_id: ruleId, error: 'Update failed' });
                }
            } catch (updateError) {
                console.error(`규칙 ${ruleId} 업데이트 실패:`, updateError.message);
                failedUpdates.push({ rule_id: ruleId, error: updateError.message });
            }
        }

        // 결과 요약
        const summary = {
            total_requested: rule_ids.length,
            successful_updates: updateResults.length,
            failed_updates: failedUpdates.length,
            updated_rules: updateResults,
            failed_rules: failedUpdates,
            applied_settings: Object.keys(settings)
        };

        console.log(`✅ 일괄 업데이트 완료: ${updateResults.length}/${rule_ids.length} 성공`);
        
        const statusCode = failedUpdates.length > 0 ? 207 : 200; // 207 Multi-Status
        res.status(statusCode).json(createResponse(true, summary, 'Bulk update completed'));

    } catch (error) {
        console.error('❌ 일괄 설정 업데이트 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'BULK_UPDATE_ERROR'));
    }
});

/**
 * GET /api/alarms/rules/:id/statistics
 * 개별 알람 규칙의 상세 통계 조회 (AlarmSettings.tsx 통계 표시용)
 */
router.get('/rules/:id/statistics', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { days = 30 } = req.query;

        console.log(`📊 알람 규칙 ${id} 통계 조회 (${days}일간)...`);

        // 알람 규칙 존재 확인
        const rule = await getAlarmRuleRepo().findById(parseInt(id), tenantId);
        if (!rule) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

        // 통계 기간 설정
        const endDate = new Date();
        const startDate = new Date();
        startDate.setDate(startDate.getDate() - parseInt(days));

        // 수정된 방법: 전체 알람 발생을 조회한 후 규칙 ID로 필터링
        let occurrences = [];
        try {
            // findAll을 사용해서 전체 조회 후 필터링
            const allOccurrences = await getAlarmOccurrenceRepo().findAll({ tenantId });
            
            // 해당 규칙의 발생만 필터링
            occurrences = allOccurrences.items ? allOccurrences.items.filter(occ => 
                occ.rule_id === parseInt(id) || occ.alarm_rule_id === parseInt(id)
            ) : [];
            
        } catch (repoError) {
            console.warn('Repository 조회 실패, 목업 데이터 사용:', repoError.message);
            // Repository 조회 실패 시 목업 데이터 사용
            occurrences = [];
        }

        // 기간 내 발생 이력 필터링
        const periodOccurrences = occurrences.filter(occ => {
            const occDate = new Date(occ.triggered_at || occ.occurrence_time);
            return occDate >= startDate && occDate <= endDate;
        });

        // 통계 계산
        const totalOccurrences = periodOccurrences.length;
        const acknowledgedOccurrences = periodOccurrences.filter(occ => 
            occ.acknowledged_time || occ.acknowledgment_time
        ).length;
        const clearedOccurrences = periodOccurrences.filter(occ => 
            occ.cleared_time || occ.clear_time
        ).length;

        // 평균 대응 시간 계산
        const acknowledgedWithTime = periodOccurrences.filter(occ => 
            (occ.acknowledged_time || occ.acknowledgment_time) && (occ.triggered_at || occ.occurrence_time)
        );
        
        let avgResponseTimeMinutes = 0;
        if (acknowledgedWithTime.length > 0) {
            const totalResponseTime = acknowledgedWithTime.reduce((sum, occ) => {
                const triggerTime = new Date(occ.triggered_at || occ.occurrence_time);
                const ackTime = new Date(occ.acknowledged_time || occ.acknowledgment_time);
                return sum + (ackTime - triggerTime);
            }, 0);
            avgResponseTimeMinutes = (totalResponseTime / acknowledgedWithTime.length) / (1000 * 60);
        }

        // 심각도별 분포
        const severityDistribution = {};
        periodOccurrences.forEach(occ => {
            const severity = occ.severity || 'unknown';
            severityDistribution[severity] = (severityDistribution[severity] || 0) + 1;
        });

        // 일별 발생 패턴 (최근 7일)
        const last7Days = [];
        for (let i = 6; i >= 0; i--) {
            const date = new Date();
            date.setDate(date.getDate() - i);
            const dayStart = new Date(date.setHours(0, 0, 0, 0));
            const dayEnd = new Date(date.setHours(23, 59, 59, 999));
            
            const dayOccurrences = periodOccurrences.filter(occ => {
                const occDate = new Date(occ.triggered_at || occ.occurrence_time);
                return occDate >= dayStart && occDate <= dayEnd;
            }).length;

            last7Days.push({
                date: dayStart.toISOString().split('T')[0],
                occurrences: dayOccurrences
            });
        }

        // 시간대별 발생 패턴 (24시간)
        const hourlyPattern = Array(24).fill(0);
        periodOccurrences.forEach(occ => {
            const hour = new Date(occ.triggered_at || occ.occurrence_time).getHours();
            hourlyPattern[hour]++;
        });

        // 마지막 발생 정보
        const latestOccurrence = periodOccurrences.sort((a, b) => 
            new Date(b.triggered_at || b.occurrence_time) - new Date(a.triggered_at || a.occurrence_time)
        )[0];

        const statistics = {
            rule_info: {
                id: rule.id,
                name: rule.name,
                is_enabled: rule.is_enabled,
                severity: rule.severity,
                created_at: rule.created_at
            },
            period: {
                start_date: startDate.toISOString(),
                end_date: endDate.toISOString(),
                days: parseInt(days)
            },
            occurrence_summary: {
                total_occurrences: totalOccurrences,
                acknowledged_count: acknowledgedOccurrences,
                cleared_count: clearedOccurrences,
                pending_count: totalOccurrences - acknowledgedOccurrences,
                acknowledgment_rate: totalOccurrences > 0 ? 
                    Math.round((acknowledgedOccurrences / totalOccurrences) * 100) : 0
            },
            performance_metrics: {
                avg_response_time_minutes: Math.round(avgResponseTimeMinutes * 10) / 10,
                frequency_per_day: totalOccurrences > 0 ? 
                    Math.round((totalOccurrences / parseInt(days)) * 10) / 10 : 0,
                last_triggered: latestOccurrence ? 
                    (latestOccurrence.triggered_at || latestOccurrence.occurrence_time) : null
            },
            distributions: {
                by_severity: severityDistribution,
                last_7_days: last7Days,
                hourly_pattern: hourlyPattern.map((count, hour) => ({ hour, count }))
            }
        };

        console.log(`✅ 알람 규칙 ${id} 통계 조회 완료`);
        res.json(createResponse(true, statistics, 'Alarm rule statistics retrieved successfully'));

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 통계 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'RULE_STATISTICS_ERROR'));
    }
});

/**
 * GET /api/alarms/rules/:id/configuration-history
 * 알람 규칙의 설정 변경 이력 조회 (AlarmSettings.tsx용)
 */
router.get('/rules/:id/configuration-history', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { limit = 20 } = req.query;

        console.log(`📜 알람 규칙 ${id} 설정 변경 이력 조회...`);

        // TODO: 실제로는 별도 audit_log 테이블이나 rule_changes 테이블에서 조회
        // 현재는 기본 정보만 반환
        const rule = await getAlarmRuleRepo().findById(parseInt(id), tenantId);
        if (!rule) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

        // 임시 목업 데이터 (실제로는 audit_log에서 조회)
        const configHistory = [
            {
                id: 1,
                rule_id: parseInt(id),
                change_type: 'settings_update',
                changed_fields: ['high_limit', 'email_notification'],
                old_values: { high_limit: 80, email_notification: false },
                new_values: { high_limit: 85, email_notification: true },
                changed_by: 'admin',
                changed_at: rule.updated_at || new Date().toISOString(),
                change_reason: 'Threshold adjustment based on operational feedback'
            }
        ];

        console.log(`✅ 설정 변경 이력 ${configHistory.length}개 조회 완료`);
        res.json(createResponse(true, {
            rule_id: parseInt(id),
            rule_name: rule.name,
            total_changes: configHistory.length,
            changes: configHistory.slice(0, parseInt(limit))
        }, 'Configuration history retrieved successfully'));

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 설정 이력 조회 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'CONFIG_HISTORY_ERROR'));
    }
});

/**
 * POST /api/alarms/rules/:id/test
 * 알람 규칙 테스트 실행 (설정 검증용)
 */
router.post('/rules/:id/test', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const { test_value, test_scenario = 'threshold' } = req.body;

        console.log(`🧪 알람 규칙 ${id} 테스트 실행...`);

        const rule = await getAlarmRuleRepo().findById(parseInt(id), tenantId);
        if (!rule) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }

        // 간단한 임계값 테스트 로직
        let testResult = {
            rule_id: parseInt(id),
            rule_name: rule.name,
            test_scenario,
            test_value,
            would_trigger: false,
            trigger_reason: null,
            recommended_action: null
        };

        if (test_scenario === 'threshold' && test_value !== undefined) {
            const highLimit = rule.high_limit;
            const lowLimit = rule.low_limit;
            
            if (highLimit && test_value > highLimit) {
                testResult.would_trigger = true;
                testResult.trigger_reason = `Test value ${test_value} exceeds high limit ${highLimit}`;
                testResult.recommended_action = 'Check if high limit setting is appropriate';
            } else if (lowLimit && test_value < lowLimit) {
                testResult.would_trigger = true;
                testResult.trigger_reason = `Test value ${test_value} is below low limit ${lowLimit}`;
                testResult.recommended_action = 'Check if low limit setting is appropriate';
            } else {
                testResult.trigger_reason = `Test value ${test_value} is within normal range`;
                testResult.recommended_action = 'Configuration appears to be working correctly';
            }
        }

        console.log(`✅ 알람 규칙 ${id} 테스트 완료`);
        res.json(createResponse(true, testResult, 'Alarm rule test completed successfully'));

    } catch (error) {
        console.error(`❌ 알람 규칙 ${req.params.id} 테스트 실패:`, error.message);
        res.status(500).json(createResponse(false, null, error.message, 'RULE_TEST_ERROR'));
    }
});

module.exports = router;