// ============================================================================
// backend/routes/devices.js (Clean Version)
// ============================================================================

const express = require('express');
const router = express.Router();
const DeviceService = require('../lib/services/DeviceService');
const {
    authenticateToken,
    tenantIsolation,
    validateTenantStatus
} = require('../middleware/tenantIsolation');

// 글로벌 미들웨어 적용
router.use(authenticateToken);
router.use(tenantIsolation);
router.use(validateTenantStatus);

// 🔍 디바이스 목록 조회
router.get('/', async (req, res) => {
    try {
        const { tenantId } = req;
        const options = {
            ...req.query,
            tenantId,
            page: parseInt(req.query.page) || 1,
            limit: parseInt(req.query.limit) || 25,
            includeRtuRelations: req.query.include_rtu_relations === 'true',
            includeCollectorStatus: req.query.include_collector_status === 'true'
        };

        const result = await DeviceService.getDevices(options);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🌳 디바이스 트리 구조 API
router.get('/tree-structure', async (req, res) => {
    try {
        const { tenantId } = req;
        const result = await DeviceService.getDeviceTree({
            tenantId,
            includeDataPoints: req.query.include_data_points === 'true',
            includeRealtime: req.query.include_realtime === 'true'
        });
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🔍 디바이스 상세 조회
router.get('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const result = await DeviceService.getDeviceById(parseInt(id), tenantId);
        res.status(result.success ? 200 : (result.message === 'Device not found' ? 404 : 500)).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// ➕ 디바이스 생성
router.post('/', async (req, res) => {
    try {
        const { tenantId } = req;
        const result = await DeviceService.createDevice(req.body, tenantId);
        res.status(result.success ? 201 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 📝 디바이스 업데이트
router.put('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const result = await DeviceService.updateDevice(parseInt(id), req.body, tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🗑️ 디바이스 삭제
router.delete('/:id', async (req, res) => {
    try {
        const { id } = req.params;
        const { tenantId } = req;
        const result = await DeviceService.delete(parseInt(id), tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

// 🚀 Collector 제어 (Start/Stop/Restart)
router.post('/:id/:action(start|stop|restart)', async (req, res) => {
    try {
        const { id, action } = req.params;
        const { tenantId } = req;
        const result = await DeviceService.executeAction(id, action, req.body, tenantId);
        res.status(result.success ? 200 : 500).json(result);
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

module.exports = router;