// =============================================================================
// backend/routes/alarms.js
// 알람 관리 전용 API 엔드포인트  
// =============================================================================

const express = require('express');
const router = express.Router();
const AlarmService = require('../services/AlarmService');
const RabbitMQService = require('../services/RabbitMQService');

// =============================================================================
// 활성 알람 관리
// =============================================================================

// GET /api/alarms/active
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
        
        const activeAlarms = await AlarmService.getActiveAlarms({
            tenantId,
            page: parseInt(page),
            limit: parseInt(limit),
            severity,
            deviceId: device_id,
            acknowledged: acknowledged === 'true'
        });
        
        res.json({
            success: true,
            data: activeAlarms.items.map(alarm => ({
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
                triggerValue: alarm.trigger_value,
                currentValue: alarm.current_value,
                location: alarm.location
            })),
            pagination: {
                page: activeAlarms.page,
                limit: activeAlarms.limit,
                total: activeAlarms.total,
                totalPages: activeAlarms.totalPages
            }
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// POST /api/alarms/:id/acknowledge
router.post('/:id/acknowledge', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment } = req.body;
        const userId = req.user.id;
        const tenantId = req.user.tenant_id;
        
        const result = await AlarmService.acknowledgeAlarm(id, userId, comment, tenantId);
        
        // WebSocket으로 실시간 알림
        req.io.to(`tenant:${tenantId}`).emit('alarm_acknowledged', {
            type: 'alarm_acknowledged',
            data: {
                alarmId: id,
                acknowledgedBy: req.user.username,
                acknowledgedAt: new Date().toISOString(),
                comment
            }
        });
        
        res.json({
            success: true,
            message: 'Alarm acknowledged successfully',
            data: result
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// POST /api/alarms/:id/clear
router.post('/:id/clear', async (req, res) => {
    try {
        const { id } = req.params;
        const { comment } = req.body;
        const userId = req.user.id;
        const tenantId = req.user.tenant_id;
        
        const result = await AlarmService.clearAlarm(id, userId, comment, tenantId);
        
        // WebSocket으로 실시간 알림
        req.io.to(`tenant:${tenantId}`).emit('alarm_cleared', {
            type: 'alarm_cleared',
            data: {
                alarmId: id,
                clearedBy: req.user.username,
                clearedAt: new Date().toISOString(),
                comment
            }
        });
        
        res.json({
            success: true,
            message: 'Alarm cleared successfully',
            data: result
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// =============================================================================
// 알람 규칙 관리
// =============================================================================

// GET /api/alarms/rules
router.get('/rules', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const { 
            page = 1, 
            limit = 20,
            target_type,
            alarm_type,
            is_enabled,
            search 
        } = req.query;
        
        const alarmRules = await AlarmService.getAlarmRules({
            tenantId,
            page: parseInt(page),
            limit: parseInt(limit),
            targetType: target_type,
            alarmType: alarm_type,
            isEnabled: is_enabled,
            search
        });
        
        res.json({
            success: true,
            data: alarmRules.items,
            pagination: {
                page: alarmRules.page,
                limit: alarmRules.limit,
                total: alarmRules.total,
                totalPages: alarmRules.totalPages
            }
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// GET /api/alarms/rules/:id
router.get('/rules/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.user.tenant_id;
        
        const alarmRule = await AlarmService.getAlarmRuleById(id, tenantId);
        if (!alarmRule) {
            return res.status(404).json({
                success: false,
                error: 'Alarm rule not found'
            });
        }
        
        res.json({
            success: true,
            data: alarmRule
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// POST /api/alarms/rules
router.post('/rules', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const ruleData = {
            ...req.body,
            tenant_id: tenantId,
            created_by: req.user.id
        };
        
        // 알람 규칙 검증
        const validation = AlarmService.validateAlarmRule(ruleData);
        if (!validation.isValid) {
            return res.status(400).json({
                success: false,
                error: 'Validation failed',
                details: validation.errors
            });
        }
        
        const newRule = await AlarmService.createAlarmRule(ruleData);
        
        // C++ Collector에 새 규칙 통지 (Redis 또는 직접 API 호출)
        await AlarmService.notifyCollectorNewRule(newRule);
        
        res.status(201).json({
            success: true,
            data: newRule
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// PUT /api/alarms/rules/:id
router.put('/rules/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.user.tenant_id;
        
        const updatedRule = await AlarmService.updateAlarmRule(id, req.body, tenantId);
        
        // C++ Collector에 규칙 변경 통지
        await AlarmService.notifyCollectorRuleUpdate(updatedRule);
        
        res.json({
            success: true,
            data: updatedRule
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// DELETE /api/alarms/rules/:id
router.delete('/rules/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const tenantId = req.user.tenant_id;
        
        await AlarmService.deleteAlarmRule(id, tenantId);
        
        // C++ Collector에 규칙 삭제 통지
        await AlarmService.notifyCollectorRuleDelete(id);
        
        res.json({
            success: true,
            message: 'Alarm rule deleted successfully'
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// =============================================================================
// 알람 이력 조회
// =============================================================================

// GET /api/alarms/history
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
            rule_id
        } = req.query;
        
        const alarmHistory = await AlarmService.getAlarmHistory({
            tenantId,
            page: parseInt(page),
            limit: parseInt(limit),
            startDate: start_date,
            endDate: end_date,
            severity,
            deviceId: device_id,
            ruleId: rule_id
        });
        
        res.json({
            success: true,
            data: alarmHistory.items,
            pagination: {
                page: alarmHistory.page,
                limit: alarmHistory.limit,
                total: alarmHistory.total,
                totalPages: alarmHistory.totalPages
            }
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// =============================================================================
// 알람 통계
// =============================================================================

// GET /api/alarms/statistics
router.get('/statistics', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const { time_range = '24h' } = req.query;
        
        const stats = await AlarmService.getAlarmStatistics(tenantId, time_range);
        
        res.json({
            success: true,
            data: {
                totalActive: stats.totalActive,
                totalToday: stats.totalToday,
                bySeverity: stats.bySeverity,
                byDevice: stats.byDevice,
                acknowledgedRate: stats.acknowledgedRate,
                avgResponseTime: stats.avgResponseTime,
                topAlarmRules: stats.topAlarmRules
            }
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// =============================================================================
// 알람 알림 설정
// =============================================================================

// GET /api/alarms/notification-settings
router.get('/notification-settings', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const userId = req.user.id;
        
        const settings = await AlarmService.getNotificationSettings(tenantId, userId);
        
        res.json({
            success: true,
            data: settings
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// POST /api/alarms/notification-settings
router.post('/notification-settings', async (req, res) => {
    try {
        const tenantId = req.user.tenant_id;
        const userId = req.user.id;
        const settings = req.body;
        
        const updatedSettings = await AlarmService.updateNotificationSettings(
            tenantId, 
            userId, 
            settings
        );
        
        res.json({
            success: true,
            data: updatedSettings
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// =============================================================================
// 대량 작업
// =============================================================================

// POST /api/alarms/bulk-acknowledge
router.post('/bulk-acknowledge', async (req, res) => {
    try {
        const { alarmIds, comment } = req.body;
        const userId = req.user.id;
        const tenantId = req.user.tenant_id;
        
        const results = await AlarmService.bulkAcknowledge(alarmIds, userId, comment, tenantId);
        
        // WebSocket으로 실시간 알림
        req.io.to(`tenant:${tenantId}`).emit('alarms_bulk_acknowledged', {
            type: 'alarms_bulk_acknowledged',
            data: {
                alarmIds,
                acknowledgedBy: req.user.username,
                count: results.successCount
            }
        });
        
        res.json({
            success: true,
            data: results
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// =============================================================================
// WebSocket 이벤트 정의
// =============================================================================

const AlarmWebSocket = {
    channels: {
        ALARM_NEW: 'alarms:new',
        ALARM_ACKNOWLEDGED: 'alarms:acknowledged',
        ALARM_CLEARED: 'alarms:cleared',
        ALARM_UPDATED: 'alarms:updated'
    },
    
    // 새 알람 발생 (C++ Collector에서 Redis를 통해 트리거)
    sendNewAlarm: (io, alarm) => {
        // 테넌트 전체에 알림
        io.to(`tenant:${alarm.tenant_id}`).emit('alarm_new', {
            type: 'alarm_triggered',
            data: alarm,
            timestamp: new Date().toISOString()
        });
        
        // 긴급 알람은 모든 사용자에게
        if (alarm.severity === 'critical') {
            io.emit('critical_alarm', {
                type: 'critical_alarm',
                data: alarm,
                timestamp: new Date().toISOString()
            });
        }
    },
    
    // 알람 상태 변경
    sendAlarmUpdate: (io, alarmUpdate) => {
        io.to(`tenant:${alarmUpdate.tenant_id}`).emit('alarm_update', {
            type: 'alarm_state_change',
            data: alarmUpdate,
            timestamp: new Date().toISOString()
        });
    }
};

// =============================================================================
// RabbitMQ 연동 (중요 알람 처리)
// =============================================================================

// 중요 알람을 RabbitMQ로 전송하여 안정적 처리
async function handleCriticalAlarm(alarm) {
    try {
        // 이메일/SMS 발송을 위해 RabbitMQ 큐에 전송
        await RabbitMQService.sendToQueue('critical_alarms', {
            type: 'critical_alarm_notification',
            alarm: alarm,
            priority: 9, // 높은 우선순위
            retry_count: 5
        });
        
        // 에스컬레이션 처리
        if (alarm.severity === 'critical') {
            await RabbitMQService.sendToQueue('alarm_escalation', {
                type: 'escalation_required',
                alarm: alarm,
                escalation_level: 1
            });
        }
        
    } catch (error) {
        console.error('Critical alarm handling failed:', error);
    }
}

module.exports = router;
module.exports.AlarmWebSocket = AlarmWebSocket;
module.exports.handleCriticalAlarm = handleCriticalAlarm;