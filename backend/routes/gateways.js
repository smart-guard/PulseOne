const express = require('express');
const router = express.Router();
const EdgeServerService = require('../lib/services/EdgeServerService');
const LogManager = require('../lib/utils/LogManager');

// =============================================================================
// Gateway (Edge Server) Management API
// =============================================================================

/**
 * @route GET /api/gateways
 * @desc 모든 게이트웨이 조회 (상태 포함)
 */
router.get('/', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;

        const response = await EdgeServerService.getAllEdgeServers(tenantId);
        const gateways = response.data || [];

        // 실시간 상태 병합 - 병렬 처리
        // 게이트웨이 수가 많지 않다고 가정 (수십 대 이내)
        const enrichedGateways = await Promise.all(gateways.map(async (gw) => {
            const liveStatus = await EdgeServerService.getLiveStatus(gw.id);
            return { ...gw, live_status: liveStatus };
        }));

        res.json({ success: true, data: enrichedGateways });
    } catch (error) {
        LogManager.api('ERROR', '게이트웨이 목록 조회 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route GET /api/gateways/:id
 * @desc 특정 게이트웨이 상세 조회
 */
router.get('/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await EdgeServerService.getEdgeServerById(req.params.id, tenantId);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 조회 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/gateways
 * @desc 신규 게이트웨이 등록
 */
router.post('/', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await EdgeServerService.registerEdgeServer(req.body, tenantId);

        LogManager.api('INFO', `게이트웨이 등록됨: ${req.body.server_name}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', '게이트웨이 등록 실패', { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route DELETE /api/gateways/:id
 * @desc 게이트웨이 삭제 (등록 해제)
 */
router.delete('/:id', async (req, res) => {
    try {
        const tenantId = req.user?.tenant_id || 1;
        const response = await EdgeServerService.unregisterEdgeServer(req.params.id, tenantId);

        LogManager.api('INFO', `게이트웨이 삭제됨: ID ${req.params.id}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 삭제 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route POST /api/gateways/:id/command
 * @desc 게이트웨이 원격 제어 명령 전송 (C2)
 */
router.post('/:id/command', async (req, res) => {
    try {
        const { id } = req.params;
        const { command, payload } = req.body;

        if (!command) {
            return res.status(400).json({ success: false, message: '명령(command)은 필수입니다.' });
        }

        const response = await EdgeServerService.sendCommand(id, command, payload);

        LogManager.api('INFO', `게이트웨이 명령 전송: ID ${id}, CMD ${command}`);
        res.json(response);
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 명령 전송 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

/**
 * @route GET /api/gateways/:id/status
 * @desc 게이트웨이 실시간 상태 조회 (Redis 직접 조회)
 */
router.get('/:id/status', async (req, res) => {
    try {
        const status = await EdgeServerService.getLiveStatus(req.params.id);

        res.json({ success: true, data: status || { status: 'offline', message: 'No heartbeat data' } });
    } catch (error) {
        LogManager.api('ERROR', `게이트웨이 실시간 상태 조회 실패 (${req.params.id})`, { error: error.message });
        res.status(500).json({ success: false, message: error.message });
    }
});

module.exports = router;
