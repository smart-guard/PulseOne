// backend/routes/services.js - 완성본 (CrossPlatformManager 단일 의존성)

const express = require('express');
const router = express.Router();
const CrossPlatformManager = require('../lib/services/crossPlatformManager');

// ========================================
// 📊 서비스 상태 조회
// ========================================

// 모든 서비스 상태 조회
router.get('/', async (req, res) => {
    try {
        const result = await CrossPlatformManager.getServicesForAPI();
        res.json(result);
    } catch (error) {
        console.error('Error fetching services:', error);
        res.status(500).json({
            success: false,
            error: 'Failed to fetch service status',
            details: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// 특정 서비스 상세 상태 조회
router.get('/:serviceName', async (req, res) => {
    try {
        const { serviceName } = req.params;
        const allServices = await CrossPlatformManager.getServicesForAPI();
        const service = allServices.data.find(s => s.name === serviceName);
    
        if (!service) {
            return res.status(404).json({
                success: false,
                error: `Service '${serviceName}' not found`,
                availableServices: allServices.data.map(s => s.name),
                timestamp: new Date().toISOString()
            });
        }

        res.json({
            success: true,
            data: {
                ...service,
                platform: CrossPlatformManager.platform,
                management: 'CrossPlatformManager',
                executablePath: service.executable,
                lastUpdated: new Date().toISOString()
            }
        });
    } catch (error) {
        console.error(`Error fetching service ${req.params.serviceName}:`, error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// ========================================
// 🎮 Collector 서비스 제어
// ========================================

// Collector 시작
router.post('/collector/start', async (req, res) => {
    try {
        console.log('🚀 Starting Collector service...');
        const result = await CrossPlatformManager.startCollector();
    
        if (result.success) {
            res.json({
                success: true,
                message: `Collector service started successfully on ${result.platform}`,
                pid: result.pid,
                platform: result.platform,
                executable: result.executable,
                timestamp: new Date().toISOString()
            });
        } else {
            res.status(400).json({
                success: false,
                error: result.error,
                platform: result.platform,
                suggestion: result.suggestion,
                buildCommand: result.buildCommand,
                development: result.development,
                timestamp: new Date().toISOString()
            });
        }
    } catch (error) {
        console.error('Error starting Collector:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// Collector 중지
router.post('/collector/stop', async (req, res) => {
    try {
        console.log('🛑 Stopping Collector service...');
        const result = await CrossPlatformManager.stopCollector();
    
        if (result.success) {
            res.json({
                success: true,
                message: `Collector service stopped successfully on ${result.platform}`,
                platform: result.platform,
                timestamp: new Date().toISOString()
            });
        } else {
            res.status(400).json({
                success: false,
                error: result.error,
                platform: result.platform,
                timestamp: new Date().toISOString()
            });
        }
    } catch (error) {
        console.error('Error stopping Collector:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// Collector 재시작
router.post('/collector/restart', async (req, res) => {
    try {
        console.log('🔄 Restarting Collector service...');
        const result = await CrossPlatformManager.restartCollector();
    
        if (result.success) {
            res.json({
                success: true,
                message: `Collector service restarted successfully on ${result.platform}`,
                pid: result.pid,
                platform: result.platform,
                executable: result.executable,
                timestamp: new Date().toISOString()
            });
        } else {
            res.status(400).json({
                success: false,
                error: result.error,
                platform: result.platform,
                timestamp: new Date().toISOString()
            });
        }
    } catch (error) {
        console.error('Error restarting Collector:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// ========================================
// 🔴 Redis 서비스 제어
// ========================================

// Redis 시작
router.post('/redis/start', async (req, res) => {
    try {
        console.log('🚀 Starting Redis service...');
        const result = await CrossPlatformManager.startRedis();
    
        if (result.success) {
            res.json({
                success: true,
                message: `Redis service started successfully on ${result.platform}`,
                pid: result.pid,
                platform: result.platform,
                port: result.port || 6379,
                timestamp: new Date().toISOString()
            });
        } else {
            res.status(400).json({
                success: false,
                error: result.error,
                platform: result.platform,
                suggestion: result.suggestion,
                timestamp: new Date().toISOString()
            });
        }
    } catch (error) {
        console.error('Error starting Redis:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// Redis 중지
router.post('/redis/stop', async (req, res) => {
    try {
        console.log('🛑 Stopping Redis service...');
        const result = await CrossPlatformManager.stopRedis();
    
        if (result.success) {
            res.json({
                success: true,
                message: `Redis service stopped successfully on ${result.platform}`,
                platform: result.platform,
                timestamp: new Date().toISOString()
            });
        } else {
            res.status(400).json({
                success: false,
                error: result.error,
                platform: result.platform,
                timestamp: new Date().toISOString()
            });
        }
    } catch (error) {
        console.error('Error stopping Redis:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// Redis 재시작
router.post('/redis/restart', async (req, res) => {
    try {
        console.log('🔄 Restarting Redis service...');
        const result = await CrossPlatformManager.restartRedis();
    
        if (result.success) {
            res.json({
                success: true,
                message: `Redis service restarted successfully on ${result.platform}`,
                pid: result.pid,
                platform: result.platform,
                port: result.port || 6379,
                timestamp: new Date().toISOString()
            });
        } else {
            res.status(400).json({
                success: false,
                error: result.error,
                platform: result.platform,
                timestamp: new Date().toISOString()
            });
        }
    } catch (error) {
        console.error('Error restarting Redis:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// Redis 상태 확인
router.get('/redis/status', async (req, res) => {
    try {
        const status = await CrossPlatformManager.getRedisStatus();
        res.json({
            success: true,
            data: status,
            timestamp: new Date().toISOString()
        });
    } catch (error) {
        console.error('Error checking Redis status:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// ========================================
// 🚫 필수 서비스 제어 차단
// ========================================

// Backend 제어 시도 시 에러 반환
router.post('/backend/:action', (req, res) => {
    res.status(403).json({
        success: false,
        error: 'Backend service is essential and cannot be controlled',
        message: 'Backend service runs as the main application process',
        suggestion: 'Use system tray icon or Windows Services manager instead',
        timestamp: new Date().toISOString()
    });
});

// Frontend 제어 시도 시 에러 반환
router.post('/frontend/:action', (req, res) => {
    res.status(403).json({
        success: false,
        error: 'Frontend is served by Backend and cannot be controlled separately',
        message: 'Frontend is integrated with the Backend service',
        suggestion: 'Access via web browser at http://localhost:3000',
        timestamp: new Date().toISOString()
    });
});

// ========================================
// 🔧 시스템 헬스 체크
// ========================================

// 전체 시스템 헬스 체크
router.get('/health/check', async (req, res) => {
    try {
        const health = await CrossPlatformManager.performHealthCheck();
    
        const statusCode = health.overall === 'healthy' ? 200 :
            health.overall === 'degraded' ? 206 : 503;

        res.status(statusCode).json({
            success: true,
            health: health,
            deployment: {
                type: health.platform === 'win32' ? 'Windows Native' : 
                    health.platform === 'linux' ? 'Linux Native' : 
                        health.platform === 'darwin' ? 'macOS Native' : 'Cross Platform',
                containerized: false,
                platform: health.platform,
                architecture: process.arch,
                nodeVersion: process.version
            }
        });
    } catch (error) {
        console.error('Error performing health check:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            health: {
                overall: 'error',
                timestamp: new Date().toISOString()
            }
        });
    }
});

// 시스템 정보 조회
router.get('/system/info', async (req, res) => {
    try {
        const systemInfo = await CrossPlatformManager.getSystemInfo();

        res.json({
            success: true,
            data: {
                ...systemInfo,
                pulseone: {
                    ...systemInfo.pulseone,
                    version: '2.1.0',
                    managedBy: 'CrossPlatformManager'
                },
                deployment: {
                    type: 'Cross Platform Native',
                    containerized: false,
                    distributed: false,
                    scalable: false,
                    supportedPlatforms: ['Windows', 'Linux', 'macOS', 'AWS', 'Docker'],
                    notes: `Ideal for industrial edge computing and SCADA applications on ${systemInfo.platform.type}`
                }
            }
        });
    } catch (error) {
        console.error('Error getting system info:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// ========================================
// 🔍 프로세스 모니터링
// ========================================

// 실행 중인 PulseOne 프로세스 목록
router.get('/processes', async (req, res) => {
    try {
        const processMap = await CrossPlatformManager.getRunningProcesses();
    
        const processes = [
            ...processMap.backend.map(p => ({
                ...p,
                service: 'backend',
                type: 'Node.js Application'
            })),
            ...processMap.collector.map(p => ({
                ...p,
                service: 'collector',
                type: 'C++ Application'
            })),
            ...processMap.redis.map(p => ({
                ...p,
                service: 'redis',
                type: 'Redis Server'
            }))
        ];

        res.json({
            success: true,
            data: {
                processes: processes,
                summary: {
                    total: processes.length,
                    backend: processMap.backend.length,
                    collector: processMap.collector.length,
                    redis: processMap.redis.length
                },
                platform: CrossPlatformManager.platform,
                architecture: CrossPlatformManager.architecture,
                timestamp: new Date().toISOString()
            }
        });
    } catch (error) {
        console.error('Error getting processes:', error);
        res.status(500).json({
            success: false,
            error: error.message,
            timestamp: new Date().toISOString()
        });
    }
});

// ========================================
// 🎯 플랫폼별 특화 기능들
// ========================================

// 플랫폼 감지 및 특화 기능 정보
router.get('/platform/info', (req, res) => {
    const platformInfo = CrossPlatformManager.getPlatformInfo();
  
    res.json({
        success: true,
        data: {
            ...platformInfo,
            managedBy: 'CrossPlatformManager',
            singleSource: true
        }
    });
});

module.exports = router;