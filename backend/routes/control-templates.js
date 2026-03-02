// backend/routes/control-templates.js
// 제어 템플릿 API (Phase 8)

'use strict';

const express = require('express');
const router = express.Router();
const { authenticateToken, validateTenantStatus } = require('../middleware/tenantIsolation');
const templateService = require('../lib/services/ControlTemplateService');

router.use(validateTenantStatus);

// GET /api/control-templates?point_id=xxx
router.get('/', authenticateToken, async (req, res) => {
    try {
        const tenant_id = req.tenantId || null;
        const { point_id } = req.query;
        const rows = await templateService.list(tenant_id, point_id);
        res.json({ success: true, data: rows });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

// POST /api/control-templates
router.post('/', authenticateToken, async (req, res) => {
    try {
        const tenant_id = req.tenantId || null;
        const tmpl = await templateService.create({ ...req.body, tenant_id });
        res.json({ success: true, data: tmpl });
    } catch (err) {
        res.status(400).json({ success: false, message: err.message });
    }
});

// PUT /api/control-templates/:id
router.put('/:id', authenticateToken, async (req, res) => {
    try {
        const tmpl = await templateService.update(parseInt(req.params.id), req.body);
        res.json({ success: true, data: tmpl });
    } catch (err) {
        res.status(400).json({ success: false, message: err.message });
    }
});

// DELETE /api/control-templates/:id
router.delete('/:id', authenticateToken, async (req, res) => {
    try {
        await templateService.remove(parseInt(req.params.id));
        res.json({ success: true });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

module.exports = router;
