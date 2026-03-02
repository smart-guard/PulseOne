// backend/routes/control-sequences.js
// 제어 시퀀스 API (Phase 4)
// GET/POST /api/control-sequences
// POST /api/control-sequences/:id/run

'use strict';

const express = require('express');
const router = express.Router();
const { authenticateToken, validateTenantStatus } = require('../middleware/tenantIsolation');
const sequenceService = require('../lib/services/ControlSequenceService');

router.use(validateTenantStatus);

// GET /api/control-sequences
router.get('/', authenticateToken, async (req, res) => {
    try {
        const tenant_id = req.tenantId || null;
        const rows = await sequenceService.list(tenant_id);
        res.json({ success: true, data: rows });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

// POST /api/control-sequences
router.post('/', authenticateToken, async (req, res) => {
    try {
        const { role } = req.user || {};
        if (role === 'viewer') {
            return res.status(403).json({ success: false, message: '제어 권한이 없습니다', error_code: 'CONTROL_PERMISSION_DENIED' });
        }
        const tenant_id = req.tenantId || null;
        const seq = await sequenceService.create({ ...req.body, tenant_id });
        res.json({ success: true, data: seq });
    } catch (err) {
        res.status(400).json({ success: false, message: err.message });
    }
});

// PUT /api/control-sequences/:id
router.put('/:id', authenticateToken, async (req, res) => {
    try {
        const { role } = req.user || {};
        if (role === 'viewer') {
            return res.status(403).json({ success: false, message: '제어 권한이 없습니다', error_code: 'CONTROL_PERMISSION_DENIED' });
        }
        const seq = await sequenceService.update(parseInt(req.params.id), req.body);
        res.json({ success: true, data: seq });
    } catch (err) {
        res.status(400).json({ success: false, message: err.message });
    }
});

// DELETE /api/control-sequences/:id
router.delete('/:id', authenticateToken, async (req, res) => {
    try {
        await sequenceService.remove(parseInt(req.params.id));
        res.json({ success: true });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

// POST /api/control-sequences/:id/run
router.post('/:id/run', authenticateToken, async (req, res) => {
    try {
        const { role, username } = req.user || {};
        if (role === 'viewer') {
            return res.status(403).json({ success: false, message: '제어 권한이 없습니다', error_code: 'CONTROL_PERMISSION_DENIED' });
        }
        const results = await sequenceService.run(parseInt(req.params.id), username || 'user');
        res.json({ success: true, data: results });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

module.exports = router;
