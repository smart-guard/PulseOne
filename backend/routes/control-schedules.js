// backend/routes/control-schedules.js
// 예약 제어 API (Phase 3)
// GET/POST/PUT/DELETE /api/control-schedules

'use strict';

const express = require('express');
const router = express.Router();
const { authenticateToken, validateTenantStatus } = require('../middleware/tenantIsolation');
const scheduler = require('../lib/services/ControlSchedulerService');

router.use(validateTenantStatus);

// GET /api/control-schedules
router.get('/', authenticateToken, async (req, res) => {
    try {
        const tenant_id = req.tenantId || null;
        const page = parseInt(req.query.page) || 1;
        const pageSize = parseInt(req.query.pageSize) || 20;
        const result = await scheduler.list(tenant_id, { page, pageSize });
        res.json({ success: true, data: result.rows, pagination: { page: result.page, pageSize: result.pageSize, totalCount: result.totalCount, totalPages: result.totalPages } });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

// POST /api/control-schedules
router.post('/', authenticateToken, async (req, res) => {
    try {
        const { role } = req.user || {};
        if (role === 'viewer') {
            return res.status(403).json({ success: false, message: '제어 권한이 없습니다', error_code: 'CONTROL_PERMISSION_DENIED' });
        }
        const tenant_id = req.tenantId || null;
        const schedule = await scheduler.create({ ...req.body, tenant_id });
        res.json({ success: true, data: schedule });
    } catch (err) {
        res.status(400).json({ success: false, message: err.message });
    }
});

// PUT /api/control-schedules/:id
router.put('/:id', authenticateToken, async (req, res) => {
    try {
        const { role } = req.user || {};
        if (role === 'viewer') {
            return res.status(403).json({ success: false, message: '제어 권한이 없습니다', error_code: 'CONTROL_PERMISSION_DENIED' });
        }
        const schedule = await scheduler.update(parseInt(req.params.id), req.body);
        res.json({ success: true, data: schedule });
    } catch (err) {
        res.status(400).json({ success: false, message: err.message });
    }
});

// DELETE /api/control-schedules/:id
router.delete('/:id', authenticateToken, async (req, res) => {
    try {
        const { role } = req.user || {};
        if (role === 'viewer') {
            return res.status(403).json({ success: false, message: '제어 권한이 없습니다', error_code: 'CONTROL_PERMISSION_DENIED' });
        }
        await scheduler.remove(parseInt(req.params.id));
        res.json({ success: true, message: '삭제되었습니다' });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

module.exports = router;
