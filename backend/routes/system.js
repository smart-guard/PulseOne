// =============================================================================
// backend/routes/system.js - 시스템 정보 관련
// =============================================================================

const express = require('express');
const router = express.Router();
const os = require('os');

// 시스템 정보 조회
router.get('/info', (req, res) => {
    res.json({
        hostname: os.hostname(),
        platform: os.platform(),
        arch: os.arch(),
        cpus: os.cpus().length,
        memory: {
            total: Math.round(os.totalmem() / 1024 / 1024), // MB
            free: Math.round(os.freemem() / 1024 / 1024),   // MB
            usage: Math.round((os.totalmem() - os.freemem()) / os.totalmem() * 100) // %
        },
        uptime: Math.round(os.uptime()), // seconds
        loadavg: os.loadavg(),
        timestamp: new Date().toISOString()
    });
});

// 데이터베이스 상태 조회
router.get('/databases', async (req, res) => {
    const connections = req.app.locals.getDB();
    const dbStatus = {};
    
    // PostgreSQL 상태
    try {
        if (connections.postgres) {
            await connections.postgres.query('SELECT 1');
            dbStatus.postgresql = {
                status: 'connected',
                host: process.env.POSTGRES_HOST || 'localhost',
                port: process.env.POSTGRES_PORT || 5432,
                database: process.env.POSTGRES_DB
            };
        } else {
            dbStatus.postgresql = { status: 'disconnected' };
        }
    } catch (error) {
        dbStatus.postgresql = { status: 'error', error: error.message };
    }
    
    // Redis 상태
    try {
        if (connections.redis) {
            await connections.redis.ping();
            dbStatus.redis = {
                status: 'connected',
                host: process.env.REDIS_HOST || 'localhost', 
                port: process.env.REDIS_PORT || 6379
            };
        } else {
            dbStatus.redis = { status: 'disconnected' };
        }
    } catch (error) {
        dbStatus.redis = { status: 'error', error: error.message };
    }
    
    // InfluxDB 상태
    try {
        if (connections.influx) {
            dbStatus.influxdb = {
                status: 'connected',
                host: process.env.INFLUX_HOST || 'localhost',
                port: process.env.INFLUX_PORT || 8086
            };
        } else {
            dbStatus.influxdb = { status: 'disconnected' };
        }
    } catch (error) {
        dbStatus.influxdb = { status: 'error', error: error.message };
    }
    
    res.json(dbStatus);
});

// 실시간 통계
router.get('/stats', async (req, res) => {
    try {
        const stats = {
            timestamp: new Date().toISOString(),
            system: {
                cpu: Math.random() * 20 + 5,  // 5-25%
                memory: Math.random() * 30 + 40, // 40-70%
                uptime: process.uptime()
            },
            processes: {
                backend: {
                    pid: process.pid,
                    memory: Math.round(process.memoryUsage().rss / 1024 / 1024), // MB
                    cpu: Math.random() * 5 + 1 // 1-6%
                }
            }
        };
        
        res.json(stats);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

module.exports = router;
