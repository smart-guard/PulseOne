const express = require('express');
const router = express.Router();
const MonitoringService = require('../lib/services/MonitoringService');

/**
 * GET /api/monitoring/system-metrics
 * 시스템 메트릭 조회
 */
router.get('/system-metrics', async (req, res) => {
    try {
        const result = await MonitoringService.getSystemMetrics();
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: 'Failed to retrieve system metrics',
            details: error.message
        });
    }
});

/**
 * GET /api/monitoring/service-health
 * 서비스 헬스체크
 */
router.get('/service-health', async (req, res) => {
    try {
        const result = await MonitoringService.getServiceHealth();
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: 'Failed to check service health',
            details: error.message
        });
    }
});

/**
 * GET /api/monitoring/database-stats
 * 데이터베이스 통계
 */
router.get('/database-stats', async (req, res) => {
    try {
        const result = await MonitoringService.getDatabaseStats();
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: 'Failed to retrieve database statistics',
            details: error.message
        });
    }
});

/**
 * GET /api/monitoring/performance
 * 성능 지표 조회
 */
router.get('/performance', async (req, res) => {
    try {
        const result = await MonitoringService.getPerformanceMetrics();
        res.json(result);
    } catch (error) {
        res.status(500).json({
            success: false,
            error: 'Failed to retrieve performance metrics',
            details: error.message
        });
    }
});

/**
 * GET /api/monitoring/logs
 * 시스템 로그 조회
 */
router.get('/logs', async (req, res) => {
    try {
        const { level = 'all', limit = 100 } = req.query;
        const result = await MonitoringService.getLogs(level, parseInt(limit));
        res.json({
            success: true,
            data: {
                logs: result.data,
                total: result.data.length,
                level,
                limit: parseInt(limit)
            }
        });
    } catch (error) {
        res.status(500).json({
            success: false,
            error: 'Failed to retrieve system logs',
            details: error.message
        });
    }
});

module.exports = router;