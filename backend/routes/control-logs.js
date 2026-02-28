// backend/routes/control-logs.js
// 제어 감사 로그 조회 API

'use strict';

const express = require('express');
const router = express.Router();
const controlLog = require('../lib/services/ControlLogService');
const { validateTenantStatus } = require('../middleware/tenantIsolation');

router.use(validateTenantStatus);

/**
 * GET /api/control-logs
 * 제어 이력 목록 조회 (필터 지원)
 * Query: device_id, point_id, user_id, tenant_id, final_status,
 *        protocol_type, from, to, page, limit
 */
router.get('/', async (req, res) => {
    try {
        const result = await controlLog.getLogs(req.query);
        res.json({ success: true, ...result });
    } catch (error) {
        console.error('[ControlLog] getLogs error:', error.message);
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * GET /api/control-logs/:id
 * 제어 이력 단건 상세 조회
 */
router.get('/:id', async (req, res) => {
    try {
        const row = await controlLog.getLogById(parseInt(req.params.id));
        if (!row) return res.status(404).json({ success: false, message: 'Not found' });
        res.json({ success: true, data: row });
    } catch (error) {
        res.status(500).json({ success: false, message: error.message });
    }
});

module.exports = router;
