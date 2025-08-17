// ============================================================================
// backend/routes/alarms.js
// 개선된 알람 관리 API - 기존 코드 + 새로운 기능 통합
// ============================================================================

const express = require('express');
const router = express.Router();
const AlarmService = require('../services/AlarmService');
const RabbitMQService = require('../services/RabbitMQService');

// ============================================================================
// 🛡️ 헬퍼 함수들 (새로 추가)
// ============================================================================

/**
 * 표준 응답 생성 함수
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
 * 로깅 헬퍼 (이모지 포함)
 */
function logInfo(message) {
    console.log(`✅ ${message}`);
}

function logError(message, error) {
    console.error(`❌ ${message}:`, error?.message || error);
}

function logStart(message) {
    console.log(`🔍 ${message} 시작...`);
}

// ============================================================================
// 🚨 활성 알람 관리 (기존 + 개선)
// ============================================================================

// GET /api/alarms/active (개선된 버전)
router.get('/active', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const { 
            page = 1, 
            limit = 50,
            severity,
            device_id,
            acknowledged = false 
        } = req.query;
        
        logStart('활성 알람 조회');

        const activeAlarms = await AlarmService.getActiveAlarms({
            tenantId,
            page: parseInt(page),
            limit: parseInt(limit),
            severity,
            deviceId: device_id,
            acknowledged: acknowledged === 'true'
        });
        
        // 표준 응답 형식으로 변환
        const result = {
            items: activeAlarms.items.map(alarm => ({
                id: alarm.id,
                ruleId: alarm.rule_id,
                ruleName: alarm.rule_name,
                deviceName: alarm.device_name,
                dataPointName: alarm.data_point_name,
                message: alarm.message,
                severity: alarm.severity,
                triggeredAt: alarm.triggered_at,
                acknowledgedAt: alarm.acknowledged_at,
                acknowledgedBy: alarm.acknowledged_by,
                isAcknowledged: !!alarm.acknowledged_at,
                currentValue: alarm.current_value,
                thresholdValue: alarm.threshold_value
            })),
            pagination: {
                page: parseInt(page),
                limit: parseInt(limit),
                total: activeAlarms.total,
                totalPages: Math.ceil(activeAlarms.total / parseInt(limit))
            }
        };
        
        logInfo(`활성 알람 ${result.items.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Active alarms retrieved successfully'));
        
    } catch (error) {
        logError('활성 알람 조회 실패', error);
        res.status(500).json(createResponse(false, null, error.message, 'ACTIVE_ALARMS_ERROR'));
    }
});

// GET /api/alarms/unacknowledged (새로 추가)
router.get('/unacknowledged', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        
        logStart('미확인 알람 조회');

        const unacknowledgedAlarms = await AlarmService.getActiveAlarms({
            tenantId,
            acknowledged: false,
            limit: 1000  // 모든 미확인 알람
        });
        
        logInfo(`미확인 알람 ${unacknowledgedAlarms.items.length}개 조회 완료`);
        res.json(createResponse(true, unacknowledgedAlarms.items, 'Unacknowledged alarms retrieved successfully'));
        
    } catch (error) {
        logError('미확인 알람 조회 실패', error);
        res.status(500).json(createResponse(false, null, error.message, 'UNACKNOWLEDGED_ALARMS_ERROR'));
    }
});

// GET /api/alarms/device/:deviceId (새로 추가)
router.get('/device/:deviceId', async (req, res) => {
    try {
        const { deviceId } = req.params;
        const tenantId = req.user.tenant_id;
        
        logStart(`디바이스 ${deviceId} 알람 조회`);

        const deviceAlarms = await AlarmService.getActiveAlarms({
            tenantId,
            deviceId: parseInt(deviceId),
            limit: 500
        });
        
        logInfo(`디바이스 ${deviceId} 알람 ${deviceAlarms.items.length}개 조회 완료`);
        res.json(createResponse(true, deviceAlarms.items, 'Device alarms retrieved successfully'));
        
    } catch (error) {
        logError(`디바이스 ${req.params.deviceId} 알람 조회 실패`, error);
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_ALARMS_ERROR'));
    }
});

// ============================================================================
// 🔧 알람 액션 (기존 + 개선)
// ============================================================================

// POST /api/alarms/:id/acknowledge (개선된 버전)
router.post('/:id/acknowledge', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment = '' } = req.body;
        const tenantId = req.user.tenant_id;
        
        logStart(`알람 ${id} 확인 처리`);

        const result = await AlarmService.acknowledgeAlarm(id, {
            acknowledged_by: req.user.id,
            acknowledgment_comment: comment,
            tenant_id: tenantId
        });

        if (!result) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm not found', 'ALARM_NOT_FOUND')
            );
        }

        // 실시간 알림 (기존 RabbitMQ + 새로운 WebSocket)
        try {
            await RabbitMQService.publishAlarmUpdate({
                type: 'alarm_acknowledged',
                alarm_id: id,
                acknowledged_by: req.user.username,
                tenant_id: tenantId
            });

            // WebSocket 지원 (있다면)
            if (req.app.get('io')) {
                req.app.get('io').to(`tenant:${tenantId}`).emit('alarm_acknowledged', {
                    type: 'alarm_acknowledged',
                    alarm_id: id,
                    acknowledged_by: req.user.username,
                    acknowledged_time: new Date().toISOString(),
                    comment
                });
            }
        } catch (notificationError) {
            console.warn('알림 발송 실패:', notificationError.message);
        }

        logInfo(`알람 ${id} 확인 처리 완료`);
        res.json(createResponse(true, result, 'Alarm acknowledged successfully'));
        
    } catch (error) {
        logError(`알람 ${req.params.id} 확인 처리 실패`, error);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_ACKNOWLEDGE_ERROR'));
    }
});

// POST /api/alarms/:id/clear (개선된 버전)
router.post('/:id/clear', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment = '', clearedValue = '' } = req.body;
        const tenantId = req.user.tenant_id;
        
        logStart(`알람 ${id} 해제 처리`);

        const result = await AlarmService.clearAlarm(id, {
            cleared_by: req.user.id,
            clear_comment: comment,
            cleared_value: clearedValue,
            tenant_id: tenantId
        });

        if (!result) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm not found', 'ALARM_NOT_FOUND')
            );
        }

        // 실시간 알림
        try {
            await RabbitMQService.publishAlarmUpdate({
                type: 'alarm_cleared',
                alarm_id: id,
                cleared_by: req.user.username,
                tenant_id: tenantId
            });

            if (req.app.get('io')) {
                req.app.get('io').to(`tenant:${tenantId}`).emit('alarm_cleared', {
                    type: 'alarm_cleared',
                    alarm_id: id,
                    cleared_by: req.user.username,
                    cleared_at: new Date().toISOString(),
                    comment
                });
            }
        } catch (notificationError) {
            console.warn('알림 발송 실패:', notificationError.message);
        }

        logInfo(`알람 ${id} 해제 처리 완료`);
        res.json(createResponse(true, result, 'Alarm cleared successfully'));
        
    } catch (error) {
        logError(`알람 ${req.params.id} 해제 처리 실패`, error);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_CLEAR_ERROR'));
    }
});

// ============================================================================
// 📜 알람 이력 관리 (기존 + 개선)
// ============================================================================

// GET /api/alarms/history (개선된 버전)
router.get('/history', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const { 
            page = 1, 
            limit = 50,
            start_date,
            end_date,
            severity,
            device_id,
            rule_id,
            state
        } = req.query;
        
        logStart('알람 이력 조회');

        const alarmHistory = await AlarmService.getAlarmHistory({
            tenantId,
            page: parseInt(page),
            limit: parseInt(limit),
            startDate: start_date,
            endDate: end_date,
            severity,
            deviceId: device_id,
            ruleId: rule_id,
            state
        });
        
        const result = {
            items: alarmHistory.items,
            pagination: {
                page: alarmHistory.page,
                limit: alarmHistory.limit,
                total: alarmHistory.total,
                totalPages: alarmHistory.totalPages
            }
        };
        
        logInfo(`알람 이력 ${result.items.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Alarm history retrieved successfully'));
        
    } catch (error) {
        logError('알람 이력 조회 실패', error);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_HISTORY_ERROR'));
    }
});

// ============================================================================
// 📊 알람 통계 (기존 + 개선)
// ============================================================================

// GET /api/alarms/statistics (개선된 버전)
router.get('/statistics', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const { time_range = '24h' } = req.query;
        
        logStart('알람 통계 조회');

        const stats = await AlarmService.getAlarmStatistics(tenantId, time_range);
        
        const result = {
            totalActive: stats.totalActive,
            totalToday: stats.totalToday,
            bySeverity: stats.bySeverity,
            byDevice: stats.byDevice,
            acknowledgedRate: stats.acknowledgedRate,
            avgResponseTime: stats.avgResponseTime,
            topAlarmRules: stats.topAlarmRules,
            // 추가 통계
            unacknowledgedCount: stats.unacknowledgedCount || 0,
            clearedToday: stats.clearedToday || 0,
            trends: stats.trends || {}
        };
        
        logInfo('알람 통계 조회 완료');
        res.json(createResponse(true, result, 'Alarm statistics retrieved successfully'));
        
    } catch (error) {
        logError('알람 통계 조회 실패', error);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_STATS_ERROR'));
    }
});

// ============================================================================
// 🔧 알람 규칙 관리 (기존 + 개선)
// ============================================================================

// GET /api/alarms/rules (개선된 버전)
router.get('/rules', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const { 
            page = 1, 
            limit = 20,
            target_type,
            alarm_type,
            is_enabled,
            severity,
            search 
        } = req.query;
        
        logStart('알람 규칙 목록 조회');

        const alarmRules = await AlarmService.getAlarmRules({
            tenantId,
            page: parseInt(page),
            limit: parseInt(limit),
            targetType: target_type,
            alarmType: alarm_type,
            isEnabled: is_enabled,
            severity,
            search
        });
        
        const result = {
            items: alarmRules.items,
            pagination: {
                page: alarmRules.page,
                limit: alarmRules.limit,
                total: alarmRules.total,
                totalPages: alarmRules.totalPages
            }
        };
        
        logInfo(`알람 규칙 ${result.items.length}개 조회 완료`);
        res.json(createResponse(true, result, 'Alarm rules retrieved successfully'));
        
    } catch (error) {
        logError('알람 규칙 조회 실패', error);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULES_ERROR'));
    }
});

// GET /api/alarms/rules/:id (개선된 버전)
router.get('/rules/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.user.tenant_id;
        
        logStart(`알람 규칙 ${id} 상세 조회`);

        const alarmRule = await AlarmService.getAlarmRuleById(id, tenantId);
        if (!alarmRule) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }
        
        logInfo(`알람 규칙 ${id} 조회 완료`);
        res.json(createResponse(true, alarmRule, 'Alarm rule retrieved successfully'));
        
    } catch (error) {
        logError(`알람 규칙 ${req.params.id} 조회 실패`, error);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_DETAIL_ERROR'));
    }
});

// POST /api/alarms/rules (개선된 버전)
router.post('/rules', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const ruleData = {
            ...req.body,
            tenant_id: tenantId,
            created_by: req.user.id
        };
        
        logStart('새 알람 규칙 생성');

        // 알람 규칙 검증 (기존 로직 유지)
        const validation = AlarmService.validateAlarmRule(ruleData);
        if (!validation.isValid) {
            return res.status(400).json(
                createResponse(false, null, 'Validation failed', 'VALIDATION_ERROR', validation.errors)
            );
        }
        
        const newRule = await AlarmService.createAlarmRule(ruleData);
        
        // C++ Collector에 새 규칙 통지 (기존 로직 유지)
        try {
            await AlarmService.notifyCollectorNewRule(newRule);
        } catch (notificationError) {
            console.warn('Collector 통지 실패:', notificationError.message);
        }
        
        logInfo(`새 알람 규칙 생성 완료: ID ${newRule.id}`);
        res.status(201).json(createResponse(true, newRule, 'Alarm rule created successfully'));
        
    } catch (error) {
        logError('알람 규칙 생성 실패', error);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_CREATE_ERROR'));
    }
});

// PUT /api/alarms/rules/:id (개선된 버전)
router.put('/rules/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.user.tenant_id;
        
        logStart(`알람 규칙 ${id} 수정`);

        const updatedRule = await AlarmService.updateAlarmRule(id, req.body, tenantId);
        
        if (!updatedRule) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }
        
        // C++ Collector에 규칙 변경 통지 (기존 로직 유지)
        try {
            await AlarmService.notifyCollectorRuleUpdate(updatedRule);
        } catch (notificationError) {
            console.warn('Collector 통지 실패:', notificationError.message);
        }
        
        logInfo(`알람 규칙 ${id} 수정 완료`);
        res.json(createResponse(true, updatedRule, 'Alarm rule updated successfully'));
        
    } catch (error) {
        logError(`알람 규칙 ${req.params.id} 수정 실패`, error);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_UPDATE_ERROR'));
    }
});

// DELETE /api/alarms/rules/:id (개선된 버전)
router.delete('/rules/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.user.tenant_id;
        
        logStart(`알람 규칙 ${id} 삭제`);

        const deleted = await AlarmService.deleteAlarmRule(id, tenantId);
        
        if (!deleted) {
            return res.status(404).json(
                createResponse(false, null, 'Alarm rule not found', 'ALARM_RULE_NOT_FOUND')
            );
        }
        
        // C++ Collector에 규칙 삭제 통지 (기존 로직 유지)
        try {
            await AlarmService.notifyCollectorRuleDelete(id);
        } catch (notificationError) {
            console.warn('Collector 통지 실패:', notificationError.message);
        }
        
        logInfo(`알람 규칙 ${id} 삭제 완료`);
        res.json(createResponse(true, { deleted: true }, 'Alarm rule deleted successfully'));
        
    } catch (error) {
        logError(`알람 규칙 ${req.params.id} 삭제 실패`, error);
        res.status(500).json(createResponse(false, null, error.message, 'ALARM_RULE_DELETE_ERROR'));
    }
});

// ============================================================================
// 🧪 테스트 및 헬스체크 (새로 추가)
// ============================================================================

// GET /api/alarms/test
router.get('/test', (req, res) => {
    res.json(createResponse(true, { 
        message: 'Alarm API is working!',
        version: '2.0.0',
        features: [
            'Enhanced logging with emojis',
            'Standardized responses',
            'Error codes',
            'WebSocket support',
            'RabbitMQ integration',
            'Unacknowledged alarms endpoint',
            'Device-specific alarms endpoint',
            'Improved filtering and pagination'
        ],
        available_endpoints: [
            'GET /api/alarms/active',
            'GET /api/alarms/unacknowledged',
            'GET /api/alarms/device/:deviceId',
            'POST /api/alarms/:id/acknowledge',
            'POST /api/alarms/:id/clear',
            'GET /api/alarms/history',
            'GET /api/alarms/statistics',
            'GET /api/alarms/rules',
            'GET /api/alarms/rules/:id',
            'POST /api/alarms/rules',
            'PUT /api/alarms/rules/:id',
            'DELETE /api/alarms/rules/:id',
            'GET /api/alarms/test'
        ]
    }, 'Alarm API test successful'));
});

// ============================================================================
// 🛡️ 에러 핸들링 미들웨어
// ============================================================================

router.use((error, req, res, next) => {
    logError('알람 라우터 예상치 못한 오류', error);
    
    res.status(500).json(createResponse(
        false, 
        null, 
        '서버 내부 오류가 발생했습니다.', 
        'INTERNAL_SERVER_ERROR'
    ));
});

module.exports = router;