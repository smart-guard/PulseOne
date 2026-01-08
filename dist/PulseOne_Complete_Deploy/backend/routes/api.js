// =============================================================================
// routes/api.js - API 정보 라우트
// =============================================================================

const express = require('express');
const router = express.Router();

// API 정보 엔드포인트
router.get('/', (req, res) => {
    res.json({
        name: 'PulseOne API',
        version: '2.1.0',
        description: 'Industrial IoT Data Collection Platform',
        endpoints: {
            health: '/api/health',
            system: '/api/system',
            devices: '/api/devices',
            alarms: '/api/alarms',
            data: '/api/data',
            processes: '/api/processes',
            services: '/api/services',
            users: '/api/users'
        },
        docs: '/api/docs',
        timestamp: new Date().toISOString()
    });
});

// API 문서 엔드포인트
router.get('/docs', (req, res) => {
    res.json({
        title: 'PulseOne API Documentation',
        version: '2.1.0',
        description: 'Complete API documentation for PulseOne Industrial IoT Platform',
        baseUrl: `${req.protocol}://${req.get('host')}/api`,
        endpoints: {
            system: {
                'GET /system/info': 'Get system information',
                'GET /system/status': 'Get system status'
            },
            devices: {
                'GET /devices': 'List all devices',
                'POST /devices': 'Create new device',
                'GET /devices/:id': 'Get device by ID',
                'PUT /devices/:id': 'Update device',
                'DELETE /devices/:id': 'Delete device'
            },
            alarms: {
                'GET /alarms/active': 'Get active alarms',
                'POST /alarms/occurrences/:id/acknowledge': 'Acknowledge alarm'
            }
        }
    });
});

module.exports = router;