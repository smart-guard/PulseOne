// ===========================================================================
// backend/routes/system.js
// ===========================================================================

const express = require('express');
const router = express.Router();
const SystemService = require('../lib/services/SystemService');

/**
 * GET /api/system/databases
 */
router.get('/databases', async (req, res) => {
    try {
        const result = await SystemService.getDatabaseStatus();
        res.json(result);
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * GET /api/system/config
 */
router.get('/config', async (req, res) => {
    try {
        const result = await SystemService.getSystemConfig();
        res.json({ success: true, data: result });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * GET /api/system/info
 */
router.get('/info', async (req, res) => {
    try {
        const result = await SystemService.getSystemInfo();
        res.json({ success: true, data: result });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * GET /api/system/health
 */
router.get('/health', (req, res) => {
    res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        uptime: process.uptime()
    });
});


/**
 * PUT /api/system/settings
 * 시스템 설정 저장. log_level 변경 시 모든 Collector에 실시간 전파
 */
router.put('/settings', async (req, res) => {
    try {
        const { logging } = req.body || {};
        const results = { saved: [], errors: [] };

        if (logging?.log_level) {
            const level = logging.log_level.toUpperCase();
            try {
                const { getInstance: getCollectorProxy } = require('../lib/services/CollectorProxyService');
                const proxy = getCollectorProxy();
                const result = await proxy.setLogLevel(level);
                results.saved.push({ key: 'log_level', value: level, ...result });
                console.log('Log level changed to ' + level);
            } catch (err) {
                console.warn('Collector propagation failed: ' + err.message);
                results.saved.push({ key: 'log_level', value: level, propagated: 0 });
            }
        }

        res.json({ success: true, results });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

module.exports = router;