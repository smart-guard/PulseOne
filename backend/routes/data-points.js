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

module.exports = router;
