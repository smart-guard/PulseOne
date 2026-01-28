const express = require('express');
const router = express.Router();
const ExportGatewayService = require('../lib/services/ExportGatewayService');
const LogManager = require('../lib/utils/LogManager');

/**
 * @route GET /api/export-gateways
 * @desc Get all export gateways
 */
router.get('/', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const page = parseInt(req.query.page) || 1;
        const limit = parseInt(req.query.limit) || 10;
        const response = await ExportGatewayService.getAllGateways(tenantId, page, limit);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '게이트웨이 목록 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/export-gateways
 * @desc Create new export gateway
 */
router.post('/', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.registerGateway(req.body, tenantId);
        LogManager.api('INFO', `게이트웨이 등록됨: ${req.body.name}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '게이트웨이 등록 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route PUT /api/export-gateways/:id
 * @desc Update export gateway
 */
router.put('/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.updateGateway(req.params.id, req.body, tenantId);
        LogManager.api('INFO', `게이트웨이 수정됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 수정 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route DELETE /api/export-gateways/:id
 * @desc Delete export gateway
 */
router.delete('/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await ExportGatewayService.deleteGateway(req.params.id, tenantId);
        LogManager.api('INFO', `게이트웨이 삭제됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 삭제 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

module.exports = router;
