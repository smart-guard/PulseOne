const express = require('express');
const router = express.Router();
const ProcessService = require('../lib/services/ProcessService');

/**
 * GET /api/processes/status
 * 모든 프로세스 상태 조회
 */
router.get('/status', async (req, res) => {
    try {
        const result = await ProcessService.getAllStatus();
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * GET /api/processes/:processName/status
 * 특정 프로세스 상태 조회
 */
router.get('/:processName/status', async (req, res) => {
    try {
        const result = await ProcessService.getSpecificStatus(req.params.processName);
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * POST /api/processes/:processName/:action
 * 프로세스 제어 (start, stop, restart)
 */
router.post('/:processName/:action', async (req, res) => {
    try {
        const { processName, action } = req.params;
        const result = await ProcessService.controlProcess(processName, action);
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

module.exports = router;