const express = require('express');
const router = express.Router();
const RealtimeService = require('../lib/services/RealtimeService');

// ============================================================================
// 🛡️ 미들웨어
// ============================================================================

const authenticateToken = (req, res, next) => {
    // 실제 환경에서는 인증 로직이 들어감
    req.user = { id: 1, tenant_id: 1, role: 'admin' };
    req.tenantId = 1;
    next();
};

const tenantIsolation = (req, res, next) => {
    if (!req.tenantId) req.tenantId = 1;
    next();
};


// 응답 헬퍼
function createResponse(success, data, message, error_code) {
    return {
        success,
        data,
        message: message || (success ? 'Success' : 'Error'),
        error_code: error_code,
        timestamp: new Date().toISOString()
    };
}

// ============================================================================
// ⚡ 실시간 데이터 API
// ============================================================================

/**
 * GET /api/realtime/current-values
 * 현재값 일괄 조회
 */
router.get('/current-values', async (req, res) => {
    try {
        const result = await RealtimeService.getCurrentValues({
            ...req.query,
            device_ids: req.query.device_ids ? req.query.device_ids.split(',') : null,
            point_names: req.query.point_names ? req.query.point_names.split(',') : null
        });
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'CURRENT_VALUES_ERROR'));
    }
});

/**
 * GET /api/realtime/device/:id/values
 * 특정 디바이스의 실시간 값 조회
 */
router.get('/device/:id/values', async (req, res) => {
    try {
        const result = await RealtimeService.getDeviceRealtimeValues(req.params.id);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'DEVICE_VALUES_ERROR'));
    }
});

/**
 * POST /api/realtime/subscribe
 * 실시간 구독 설정
 */
router.post('/subscribe', async (req, res) => {
    try {
        const result = await RealtimeService.createSubscription(req.body, req.tenantId, req.user.id);
        res.json(result);
    } catch (error) {
        res.status(500).json(createResponse(false, null, error.message, 'SUBSCRIPTION_CREATE_ERROR'));
    }
});

module.exports = router;
