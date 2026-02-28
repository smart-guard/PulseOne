const express = require('express');
const router = express.Router({ mergeParams: true });
const DataPointService = require('../lib/services/DataPointService');
const {
    authenticateToken,
    tenantIsolation,
    validateTenantStatus
} = require('../middleware/tenantIsolation');

// 글로벌 미들웨어 적용
router.use(validateTenantStatus);

/**
 * GET /api/devices/:deviceId/data-points
 * 디바이스별 데이터포인트 목록 조회
 */
router.get('/', async (req, res) => {
    try {
        const { deviceId } = req.params;
        const result = await DataPointService.getDeviceDataPoints(parseInt(deviceId), req.query);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/devices/:deviceId/data-points/bulk
 * 데이터포인트 일괄 저장 (Upsert)
 */
router.post('/bulk', async (req, res) => {
    try {
        const { deviceId } = req.params;
        const { data_points } = req.body;
        const { tenantId } = req;

        if (!data_points || !Array.isArray(data_points)) {
            return res.status(400).json({ success: false, message: 'data_points array is required' });
        }

        const result = await DataPointService.bulkUpsert(parseInt(deviceId), data_points, tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * POST /api/devices/:deviceId/data-points/:pointId/write
 * 데이터포인트 제어값 쓰기 → 감사 로그 기록 → Redis Pub → Collector → 실제 장비
 * Body: { "value": "75" }
 */
router.post('/:pointId/write', async (req, res) => {
    try {
        const { deviceId, pointId } = req.params;
        const { value } = req.body;

        if (value === undefined || value === null || value === '') {
            return res.status(400).json({ success: false, message: 'value is required' });
        }

        const redisClient = require('../lib/connection/redis');
        const knex = require('../lib/database/KnexManager').getInstance().getKnex();
        const controlLog = require('../lib/services/ControlLogService');

        // ── 1. 포인트 조회 + access_mode 검증 ────────────────────
        const point = await knex('data_points')
            .where({ id: parseInt(pointId), device_id: parseInt(deviceId) })
            .first();

        if (!point) {
            return res.status(404).json({ success: false, message: 'Data point not found' });
        }

        if (point.access_mode === 'read') {
            return res.status(403).json({
                success: false,
                message: `포인트 "${point.name}"는 읽기 전용입니다 (access_mode=read)`
            });
        }

        // ── 2. 디바이스 → edge_server_id, tenant_id, site_id ────
        const device = await knex('devices').where({ id: parseInt(deviceId) }).first();
        if (!device) {
            return res.status(404).json({ success: false, message: 'Device not found' });
        }

        const edgeServerId = device.edge_server_id;
        if (!edgeServerId) {
            return res.status(422).json({
                success: false,
                message: '디바이스에 할당된 Collector가 없습니다 (edge_server_id is null)'
            });
        }

        // ── 3. 기존값(old_value) 조회 ───────────────────────────
        let old_value = null;
        try {
            const cv = await knex('current_values').where({ point_id: parseInt(pointId) }).first();
            if (cv?.current_value) {
                try {
                    const parsed = JSON.parse(cv.current_value);
                    old_value = parsed?.value != null ? String(parsed.value) : String(cv.current_value);
                } catch {
                    old_value = String(cv.current_value);
                }
            }
        } catch (e) { /* current_values 없을 수도 있음 */ }

        // ── 4. 사용자 정보 추출 ──────────────────────────────────
        const user_id = req.user?.id || req.userId || null;
        const username = req.user?.username || req.user?.name || 'unknown';
        const tenant_id = req.tenantId || device.tenant_id || null;
        const site_id = device.site_id || null;

        // ── 5. 제어 로그 INSERT (pending) ───────────────────────
        const request_id = await controlLog.createLog({
            tenant_id, site_id, user_id, username,
            device_id: parseInt(deviceId),
            device_name: device.name,
            protocol_type: device.protocol_type || null,
            point_id: parseInt(pointId),
            point_name: point.name,
            address: point.address_string || String(point.address),
            old_value,
            requested_value: String(value)
        });

        // ── 6. Redis PUBLISH ─────────────────────────────────────
        const channel = `cmd:collector:${edgeServerId}`;
        const message = JSON.stringify({
            command: 'write',
            device_id: String(deviceId),
            point_id: String(pointId),
            value: String(value),
            request_id,                      // ← 감사 로그 추적용 UUID
            timestamp: new Date().toISOString()
        });

        const client = await redisClient.getRedisClient();
        if (!client) {
            await controlLog.markDelivery(request_id, 0);
            return res.status(503).json({ success: false, message: 'Redis를 사용할 수 없습니다' });
        }

        const subscriberCount = await client.publish(channel, message);

        // ── 7. 전달 상태 업데이트 ────────────────────────────────
        await controlLog.markDelivery(request_id, subscriberCount);

        console.log(`📡 [PointWrite] → ${channel} | device=${deviceId} point=${pointId}(${point.name}) value=${value} | request_id=${request_id} | subscribers=${subscriberCount}`);

        res.json({
            success: true,
            message: subscriberCount > 0
                ? '제어 명령이 Collector로 전송되었습니다'
                : '⚠️ 연결된 Collector가 없습니다 (명령 미전달)',
            data: {
                request_id,
                point_id: parseInt(pointId),
                point_name: point.name,
                device_id: parseInt(deviceId),
                old_value,
                requested_value: String(value),
                subscriber_count: subscriberCount
            }
        });

    } catch (error) {
        console.error('[PointWrite] Error:', error);
        res.status(500).json({ success: false, message: error.message });
    }
});

module.exports = router;
