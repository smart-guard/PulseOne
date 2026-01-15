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

module.exports = router;