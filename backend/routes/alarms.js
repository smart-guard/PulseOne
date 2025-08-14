// ============================================================================
// backend/routes/alarms.js
// 완전한 알람 관리 API - Repository 패턴 활용 (완성본)
// ============================================================================

const express = require('express');
const router = express.Router();

// Repository imports (실제 구현한 것들 사용)
const AlarmRuleRepository = require('../lib/database/repositories/AlarmRuleRepository');
const AlarmOccurrenceRepository = require('../lib/database/repositories/AlarmOccurrenceRepository');

// Repository 인스턴스 생성
let alarmRuleRepo = null;
let alarmOccurrenceRepo = null;

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
            'GET /api/alarms/test'
        ]
    }, 'Test successful'));
});

module.exports = router;