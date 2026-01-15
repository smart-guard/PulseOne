// =============================================================================
// backend/routes/errors.js
// =============================================================================

const express = require('express');
const router = express.Router();
const ErrorMonitoringService = require('../lib/services/ErrorMonitoringService');

/**
 * GET /api/errors/statistics
 */
router.get('/statistics', async (req, res) => {
    try {
        const result = await ErrorMonitoringService.getStatistics();
        res.json({
            success: true,
            data: result,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * GET /api/errors/recent
 */
router.get('/recent', async (req, res) => {
    try {
        const limit = parseInt(req.query.limit) || 50;
        const { category, device_id: deviceId } = req.query;
        const result = await ErrorMonitoringService.getRecentErrors(limit, category, deviceId);
        res.json({
            success: true,
            data: result,
            total: result.length,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

/**
 * POST /api/errors/report
 */
router.post('/report', async (req, res) => {
    try {
        const result = await ErrorMonitoringService.reportError(req.body);
        res.json({
            success: true,
            message: 'Error recorded successfully',
            error_id: result.id
        });
    } catch (error) {
        res.status(500).json({ success: false, error: error.message });
    }
});

module.exports = router;