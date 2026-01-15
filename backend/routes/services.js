const express = require('express');
const router = express.Router();
const ProcessService = require('../lib/services/ProcessService');

/**
 * GET /api/services
 * 모든 서비스 상태 조회
 */
router.get('/', async (req, res) => {
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
 * GET /api/services/:serviceName
 * 특정 서비스 상세 상태 조회
 */
router.get('/:serviceName', async (req, res) => {
    try {
        const result = await ProcessService.getSpecificStatus(req.params.serviceName);
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * POST /api/services/:processName/start
 * Collector/Redis 시작
 */
router.post('/:processName/start', async (req, res) => {
    try {
        const result = await ProcessService.controlProcess(req.params.processName, 'start');
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * POST /api/services/:processName/stop
 * Collector/Redis 중지
 */
router.post('/:processName/stop', async (req, res) => {
    try {
        const result = await ProcessService.controlProcess(req.params.processName, 'stop');
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * POST /api/services/:processName/restart
 * Collector/Redis 재시작
 */
router.post('/:processName/restart', async (req, res) => {
    try {
        const result = await ProcessService.controlProcess(req.params.processName, 'restart');
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

/**
 * GET /api/services/platform/info
 * 플랫폼 정보 조회
 */
router.get('/platform/info', async (req, res) => {
    try {
        const result = await ProcessService.getPlatformInfo();
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
});

module.exports = router;