// =============================================================================
// backend/routes/collectors.js
// Collector 인스턴스(Edge Servers) 관리 라우트
// =============================================================================

const express = require('express');
const router = express.Router();
const EdgeServerService = require('../lib/services/EdgeServerService');
const { getInstance: getCollectorProxy } = require('../lib/services/CollectorProxyService');
const logger = require('../lib/utils/LogManager');

// 모든 Collector 인스턴스 조회
router.get('/', async (req, res) => {
    try {
        const result = await EdgeServerService.getAllEdgeServers(req.tenantId);
        res.json(result);
    } catch (error) {
        logger.api('ERROR', 'Failed to list collectors', { error: error.message });
        res.status(500).json({ success: false, error: error.message });
    }
});

// 활성 Collector 목록 조회
router.get('/active', async (req, res) => {
    try {
        const result = await EdgeServerService.getActiveEdgeServers(req.tenantId);
        res.json(result);
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// Collector 상세 조회 및 실시간 상태
router.get('/:id', async (req, res) => {
    try {
        const server = await EdgeServerService.getEdgeServerById(req.params.id, req.tenantId);

        // Proxy를 통한 실시간 상태 병합
        const proxy = getCollectorProxy();
        let stats = null;
        try {
            const statusResult = await proxy.getSystemStatistics(req.params.id);
            stats = statusResult.data;
        } catch (e) {
            logger.api('WARN', `Collector [${req.params.id}] stats fetch failed`, { error: e.message });
        }

        res.json({
            success: true,
            data: {
                ...server.data,
                live_stats: stats
            }
        });
    } catch (error) {
        res.status(error.message.includes('not found') ? 404 : 500).json({ success: false, error: error.message });
    }
});

// Collector 등록
router.post('/', async (req, res) => {
    try {
        const result = await EdgeServerService.registerEdgeServer(req.body, req.tenantId, req.user);
        res.status(201).json(result);
    } catch (error) {
        res.status(400).json({ success: false, error: error.message });
    }
});

// Collector 상태 업데이트 (하트비트 등)
router.patch('/:id/status', async (req, res) => {
    try {
        const { status, remarks } = req.body;
        const result = await EdgeServerService.updateEdgeServerStatus(req.params.id, status, remarks);
        res.json(result);
    } catch (error) {
        res.status(400).json({ success: false, error: error.message });
    }
});

// Collector 인스턴스 건강상태 체크 (실시간)
router.get('/:id/health', async (req, res) => {
    try {
        const proxy = getCollectorProxy();
        const health = await proxy.healthCheck(req.params.id);
        res.json(health);
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

// Collector 삭제 (등록 해제)
router.delete('/:id', async (req, res) => {
    try {
        const result = await EdgeServerService.unregisterEdgeServer(req.params.id, req.tenantId, req.user);
        res.json(result);
    } catch (error) {
        res.status(400).json({ success: false, error: error.message });
    }
});

module.exports = router;
