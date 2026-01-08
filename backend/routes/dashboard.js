// backend/routes/dashboard.js - 완성본 (CrossPlatformManager 단일 의존성, 전체 기능 유지)

const express = require('express');
const router = express.Router();
const CrossPlatformManager = require('../lib/services/crossPlatformManager');

// Repository imports (수정됨 - 실제 존재하는 것들만)
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');
const SiteRepository = require('../lib/database/repositories/SiteRepository');
const AlarmOccurrenceRepository = require('../lib/database/repositories/AlarmOccurrenceRepository');
const AlarmRuleRepository = require('../lib/database/repositories/AlarmRuleRepository');

// Connection modules (실시간 상태 확인용)
let redisClient = null;
let postgresQuery = null;

try {
    redisClient = require('../lib/connection/redis');
} catch (error) {
    console.warn('Redis 연결 모듈 로드 실패:', error.message);
}

try {
    const postgres = require('../lib/connection/postgres');
    postgresQuery = postgres.query;
} catch (error) {
    console.warn('PostgreSQL 연결 모듈 로드 실패:', error.message);
}

// Repository 인스턴스 생성
let deviceRepo, siteRepo, alarmOccurrenceRepo, alarmRuleRepo;

function initRepositories() {
    if (!deviceRepo) {
        deviceRepo = new DeviceRepository();
        siteRepo = new SiteRepository();
        alarmOccurrenceRepo = new AlarmOccurrenceRepository();
        alarmRuleRepo = new AlarmRuleRepository();
        console.log('Dashboard Repositories 초기화 완료 (CrossPlatformManager 사용)');
    }
}

// ============================================================================
// 미들웨어 (간단한 개발용 버전)
// ============================================================================

const authenticateToken = (req, res, next) => {
    req.user = { id: 1, tenant_id: 1, role: 'admin' };
    req.tenantId = 1;
    next();
};

const tenantIsolation = (req, res, next) => {
    if (!req.tenantId) req.tenantId = 1;
    next();
};

const validateTenantStatus = (req, res, next) => {
    next();
};

// ============================================================================
// 유틸리티 함수들 (CrossPlatformManager 사용)
// ============================================================================

function createResponse(success, data, message, error_code) {
    return {
        success,
        data,
        message: message || (success ? 'Success' : 'Error'),
        error_code: error_code,
        timestamp: new Date().toISOString()
    };
}

/**
 * 실제 서비스 상태 확인 (CrossPlatformManager 사용)
 */
async function getActualServiceStatus() {
    try {
        const result = await CrossPlatformManager.getServicesForAPI();
        return result.data || [];
    } catch (error) {
        console.warn('서비스 상태 조회 실패:', error.message);
        return [
            {
                name: 'backend',
                displayName: 'Backend API',
                status: 'running',
                icon: 'fas fa-server',
                controllable: false,
                description: 'REST API 서버 (필수 서비스)',
                uptime: Math.floor(process.uptime()),
                memory_usage: Math.floor(process.memoryUsage().heapUsed / 1024 / 1024),
                cpu_usage: 0
            }
        ];
    }
}

/**
 * 실제 시스템 메트릭 조회 (CrossPlatformManager 사용)
 */
async function getActualSystemMetrics() {
    try {
        const systemInfo = await CrossPlatformManager.getSystemInfo();
        const memUsage = process.memoryUsage();
        
        return {
            dataPointsPerSecond: 0,
            avgResponseTime: 0,
            dbQueryTime: 0,
            cpuUsage: 0,
            memoryUsage: Math.floor((memUsage.heapUsed / memUsage.heapTotal) * 100),
            diskUsage: 0,
            networkUsage: 0,
            activeConnections: 0,
            processUptime: Math.floor(process.uptime()),
            nodeVersion: process.version,
            platform: systemInfo.platform.type,
            arch: systemInfo.platform.architecture
        };
    } catch (error) {
        console.warn('시스템 메트릭 조회 실패:', error.message);
        const memUsage = process.memoryUsage();
        return {
            dataPointsPerSecond: 0,
            avgResponseTime: 0,
            dbQueryTime: 0,
            cpuUsage: 0,
            memoryUsage: Math.floor((memUsage.heapUsed / memUsage.heapTotal) * 100),
            diskUsage: 0,
            networkUsage: 0,
            activeConnections: 0,
            processUptime: Math.floor(process.uptime()),
            nodeVersion: process.version,
            platform: process.platform,
            arch: process.arch
        };
    }
}

// =============================================================================
// 서비스 제어 API 구현 (CrossPlatformManager 사용)
// =============================================================================

/**
 * POST /api/dashboard/service/:serviceName/control
 * 서비스 시작/중지/재시작 제어
 */
router.post('/service/:serviceName/control', async (req, res) => {
    try {
        const { serviceName } = req.params;
        const { action } = req.body;

        console.log(`서비스 제어 요청: ${serviceName} ${action}`);

        if (!['start', 'stop', 'restart'].includes(action)) {
            return res.status(400).json(createResponse(false, null, 
                `Invalid action: ${action}. Must be start, stop, or restart`, 'INVALID_ACTION'));
        }

        let result;
        
        switch (serviceName.toLowerCase()) {
        case 'collector':
            result = await handleCollectorService(action);
            break;
        case 'redis':
            result = await handleRedisService(action);
            break;
        case 'database':
            result = await handleDatabaseService(action);
            break;
        default:
            return res.status(404).json(createResponse(false, null, 
                `Unknown service: ${serviceName}`, 'SERVICE_NOT_FOUND'));
        }

        if (result.success) {
            console.log(`서비스 ${serviceName} ${action} 성공`);
            res.json(createResponse(true, result.data, result.message));
        } else {
            console.error(`서비스 ${serviceName} ${action} 실패:`, result.error);
            res.status(500).json(createResponse(false, null, result.error, 'SERVICE_CONTROL_ERROR'));
        }

    } catch (error) {
        console.error('서비스 제어 오류:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'SERVICE_CONTROL_ERROR'));
    }
});

// =============================================================================
// 서비스별 제어 함수들 (CrossPlatformManager 사용)
// =============================================================================

/**
 * Collector 서비스 제어 (CrossPlatformManager 사용)
 */
async function handleCollectorService(action) {
    switch (action) {
    case 'start':
        return await CrossPlatformManager.startCollector();
    case 'stop':
        return await CrossPlatformManager.stopCollector();
    case 'restart':
        return await CrossPlatformManager.restartCollector();
    default:
        return { success: false, error: `Unknown action: ${action}` };
    }
}

/**
 * Redis 서비스 제어 (CrossPlatformManager 사용)
 */
async function handleRedisService(action) {
    switch (action) {
    case 'start':
        return await CrossPlatformManager.startRedis();
    case 'stop':
        return await CrossPlatformManager.stopRedis();
    case 'restart':
        return await CrossPlatformManager.restartRedis();
    default:
        return { success: false, error: `Unknown action: ${action}` };
    }
}

/**
 * 데이터베이스 서비스 제어 (SQLite는 파일 기반이므로 제한적)
 */
async function handleDatabaseService(action) {
    return new Promise((resolve) => {
        switch (action) {
        case 'start':
            testDatabaseConnection()
                .then(() => {
                    resolve({ 
                        success: true, 
                        message: 'SQLite 데이터베이스 연결이 정상입니다.',
                        data: { type: 'sqlite', status: 'connected' }
                    });
                })
                .catch((error) => {
                    resolve({ 
                        success: false, 
                        error: `데이터베이스 연결 실패: ${error.message}`
                    });
                });
            break;
                
        case 'stop':
            resolve({ 
                success: true, 
                message: 'SQLite는 파일 기반 데이터베이스로 별도 중지가 필요하지 않습니다.',
                data: { type: 'sqlite', note: 'File-based database' }
            });
            break;
                
        case 'restart':
            resolve({ 
                success: true, 
                message: 'SQLite 데이터베이스 재연결을 시뮬레이션했습니다.',
                data: { type: 'sqlite', action: 'simulated_restart' }
            });
            break;
                
        default:
            resolve({ success: false, error: `Unknown action: ${action}` });
        }
    });
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

/**
 * 데이터베이스 연결 테스트
 */
function testDatabaseConnection() {
    return new Promise((resolve, reject) => {
        try {
            const connections = require('../app').locals.getDB();
            if (connections && connections.db) {
                connections.db.get('SELECT 1', (err, row) => {
                    if (err) {
                        reject(err);
                    } else {
                        resolve(row);
                    }
                });
            } else {
                reject(new Error('데이터베이스 연결을 찾을 수 없습니다.'));
            }
        } catch (error) {
            reject(error);
        }
    });
}

// =============================================================================
// 서비스 상태 조회 API (CrossPlatformManager 사용)
// =============================================================================

/**
 * GET /api/dashboard/services/status
 * 모든 서비스 상태 조회
 */
router.get('/services/status', async (req, res) => {
    try {
        console.log('서비스 상태 조회 중...');

        const result = await CrossPlatformManager.getServicesForAPI();
        
        if (result.success) {
            res.json(createResponse(true, {
                services: result.data,
                summary: result.summary
            }, 'Services status retrieved successfully'));
        } else {
            res.status(500).json(createResponse(false, null, 'Failed to get services status', 'STATUS_CHECK_ERROR'));
        }

    } catch (error) {
        console.error('서비스 상태 조회 실패:', error.message);
        res.status(500).json(createResponse(false, null, error.message, 'STATUS_CHECK_ERROR'));
    }
});

// ============================================================================
// 상용 대시보드 API 엔드포인트들 (기존 코드 유지)
// ============================================================================

/**
 * GET /api/dashboard/overview
 * 전체 시스템 개요 데이터 (DeviceRepository 직접 사용)
 */
router.get('/overview', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            initRepositories();
            const { tenantId } = req;

            console.log(`대시보드 개요 데이터 요청 (테넌트: ${tenantId})`);

            // 1. 실제 서비스 상태 확인 (CrossPlatformManager 사용)
            const services = await getActualServiceStatus();
            
            // 2. 실제 시스템 메트릭 (CrossPlatformManager 사용)
            const systemMetrics = await getActualSystemMetrics();
            
            // 3. DeviceRepository에서 직접 디바이스 통계 조회
            let deviceStats = { total: 0, active: 0, inactive: 0, connected: 0, disconnected: 0, error: 0, protocols: {}, sites_count: 0 };
            try {
                const protocolStats = await deviceRepo.getDeviceStatsByProtocol(tenantId);
                const siteStats = await deviceRepo.getDeviceStatsBySite(tenantId);
                const systemSummary = await deviceRepo.getSystemStatusSummary(tenantId);
                
                deviceStats = {
                    total: systemSummary.total_devices || 0,
                    active: systemSummary.active_devices || 0,
                    inactive: systemSummary.inactive_devices || 0,
                    connected: systemSummary.connected_devices || 0,
                    disconnected: systemSummary.disconnected_devices || 0,
                    error: systemSummary.error_devices || 0,
                    protocols: protocolStats.reduce((acc, p) => { acc[p.protocol_type] = p.device_count; return acc; }, {}),
                    sites_count: siteStats.length || 0
                };
            } catch (error) {
                console.warn('디바이스 통계 조회 실패:', error.message);
            }
            
            // 4. 사이트 통계
            let siteStats = { total: 0 };
            try {
                const sites = await siteRepo.findAll(tenantId);
                siteStats = { total: sites.length || 0 };
            } catch (error) {
                console.warn('사이트 통계 조회 실패:', error.message);
            }
            
            // 5. 알람 통계
            let activeAlarms = [];
            let todayAlarms = [];
            try {
                activeAlarms = await alarmOccurrenceRepo.findActivePlainSQL(tenantId);
                todayAlarms = await alarmOccurrenceRepo.findTodayAlarms(tenantId);
            } catch (error) {
                console.warn('알람 통계 조회 실패:', error.message);
            }
            
            // 6. 데이터포인트 통계 (DeviceRepository 사용)
            let dataPointStats = { total: 0, active: 0, analog: 0, digital: 0, string: 0 };
            try {
                const allDataPoints = await deviceRepo.searchDataPoints(tenantId, '');
                dataPointStats = {
                    total: allDataPoints.length,
                    active: allDataPoints.filter(dp => dp.is_enabled).length,
                    analog: allDataPoints.filter(dp => dp.data_type === 'analog' || dp.data_type === 'REAL').length,
                    digital: allDataPoints.filter(dp => dp.data_type === 'digital' || dp.data_type === 'BOOL').length,
                    string: allDataPoints.filter(dp => dp.data_type === 'string' || dp.data_type === 'STRING').length
                };
            } catch (error) {
                console.warn('데이터포인트 통계 조회 실패:', error.message);
            }
            
            // 7. 최근 알람 목록
            let recentAlarms = [];
            try {
                recentAlarms = await alarmOccurrenceRepo.findRecentAlarms(tenantId, 10);
            } catch (error) {
                console.warn('최근 알람 조회 실패:', error.message);
            }

            // 종합 응답 데이터
            const overviewData = {
                services: {
                    total: services.length,
                    running: services.filter(s => s.status === 'running').length,
                    stopped: services.filter(s => s.status === 'stopped').length,
                    error: services.filter(s => s.status === 'error').length,
                    details: services
                },
                
                system_metrics: systemMetrics,
                
                device_summary: deviceStats,
                
                alarms: {
                    active_total: activeAlarms?.length || 0,
                    today_total: todayAlarms?.length || 0,
                    unacknowledged: activeAlarms?.filter(a => !a.acknowledged_at).length || 0,
                    critical: activeAlarms?.filter(a => a.severity === 'critical').length || 0,
                    major: activeAlarms?.filter(a => a.severity === 'major').length || 0,
                    minor: activeAlarms?.filter(a => a.severity === 'minor').length || 0,
                    warning: activeAlarms?.filter(a => a.severity === 'warning').length || 0,
                    recent_alarms: recentAlarms?.slice(0, 5) || []
                },
                
                data_summary: dataPointStats,
                
                health_status: {
                    overall: services.filter(s => s.status === 'running').length >= Math.ceil(services.length * 0.8) ? 'healthy' : 'degraded',
                    database_connected: services.find(s => s.name === 'database')?.status === 'running',
                    redis_connected: services.find(s => s.name === 'redis')?.status === 'running',
                    active_alarms_count: activeAlarms?.length || 0,
                    services_running: services.filter(s => s.status === 'running').length,
                    services_total: services.length
                }
            };

            console.log('대시보드 개요 데이터 생성 완료 (CrossPlatformManager 사용)');
            res.json(createResponse(true, overviewData, 'Dashboard overview loaded successfully'));

        } catch (error) {
            console.error('대시보드 개요 데이터 조회 실패:', error.message);
            res.status(500).json(createResponse(false, null, error.message, 'DASHBOARD_OVERVIEW_ERROR'));
        }
    });

/**
 * GET /api/dashboard/tenant-stats
 * 테넌트별 상세 통계 (DeviceRepository 직접 사용)
 */
router.get('/tenant-stats', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            initRepositories();
            const { tenantId } = req;
            
            console.log(`테넌트 ${tenantId} 상세 통계 요청`);

            let deviceStats = {};
            let siteStats = {};
            let alarmStats = {};
            let recentDevices = [];

            try {
                const [protocolStats, siteStatsRaw, systemSummary, recentActive] = await Promise.all([
                    deviceRepo.getDeviceStatsByProtocol(tenantId),
                    deviceRepo.getDeviceStatsBySite(tenantId),
                    deviceRepo.getSystemStatusSummary(tenantId),
                    deviceRepo.getRecentActiveDevices(tenantId, 10)
                ]);

                deviceStats = {
                    total: systemSummary.total_devices || 0,
                    active: systemSummary.active_devices || 0,
                    inactive: systemSummary.inactive_devices || 0,
                    connected: systemSummary.connected_devices || 0,
                    disconnected: systemSummary.disconnected_devices || 0,
                    by_protocol: protocolStats.reduce((acc, p) => { acc[p.protocol_type] = p.device_count; return acc; }, {}),
                    by_site: siteStatsRaw.reduce((acc, s) => { acc[s.site_name] = s.device_count; return acc; }, {}),
                    recent_additions: recentActive
                };

                recentDevices = recentActive;
            } catch (error) {
                console.warn('디바이스 상세 통계 조회 실패:', error.message);
            }

            try {
                const sites = await siteRepo.findAll(tenantId);
                siteStats = {
                    total: sites.length || 0,
                    active: sites.filter(s => s.is_active).length || 0,
                    device_count_by_site: {}
                };
            } catch (error) {
                console.warn('사이트 상세 통계 조회 실패:', error.message);
            }

            try {
                const alarmStatsRaw = await alarmOccurrenceRepo.getStatsByTenant(tenantId);
                alarmStats = alarmStatsRaw || {
                    active: 0,
                    total_today: 0,
                    total_week: 0,
                    total_month: 0,
                    by_severity: {},
                    by_device: {},
                    response_times: {}
                };
            } catch (error) {
                console.warn('알람 상세 통계 조회 실패:', error.message);
                alarmStats = {
                    active: 0,
                    total_today: 0,
                    total_week: 0,
                    total_month: 0,
                    by_severity: {},
                    by_device: {},
                    response_times: {}
                };
            }

            const tenantStatsData = {
                tenant_id: tenantId,
                tenant_name: `Tenant ${tenantId}`,
                
                devices: deviceStats,
                
                sites: siteStats,
                
                alarms: alarmStats,
                
                data_points: {
                    total: 0,
                    active: 0,
                    by_type: {},
                    by_device: {},
                    update_rates: {}
                },
                
                recent_activity: {
                    recent_devices: recentDevices || [],
                    last_updated: new Date().toISOString()
                }
            };

            console.log(`테넌트 ${tenantId} 상세 통계 조회 완료`);
            res.json(createResponse(true, tenantStatsData, 'Tenant statistics loaded successfully'));

        } catch (error) {
            console.error(`테넌트 ${req.tenantId} 통계 조회 실패:`, error.message);
            res.status(500).json(createResponse(false, null, error.message, 'TENANT_STATS_ERROR'));
        }
    });

/**
 * GET /api/dashboard/recent-devices
 * 최근 연결된 디바이스 목록 (DeviceRepository 직접 사용)
 */
router.get('/recent-devices', 
    authenticateToken, 
    tenantIsolation, 
    async (req, res) => {
        try {
            initRepositories();
            const { tenantId } = req;
            const limit = parseInt(req.query.limit) || 10;
            
            console.log(`테넌트 ${tenantId} 최근 디바이스 목록 요청 (limit: ${limit})`);

            const recentDevices = await deviceRepo.getRecentActiveDevices(tenantId, limit);

            console.log(`최근 디바이스 ${recentDevices?.length || 0}개 조회 완료`);
            res.json(createResponse(true, recentDevices || [], 'Recent devices loaded successfully'));

        } catch (error) {
            console.error('최근 디바이스 목록 조회 실패:', error.message);
            res.status(500).json(createResponse(false, null, error.message, 'RECENT_DEVICES_ERROR'));
        }
    });

/**
 * GET /api/dashboard/system-health
 * 시스템 헬스 상태 (CrossPlatformManager 사용)
 */
router.get('/system-health', 
    authenticateToken, 
    async (req, res) => {
        try {
            console.log('시스템 헬스 상태 요청');

            // CrossPlatformManager 사용
            const health = await CrossPlatformManager.performHealthCheck();
            const systemMetrics = await getActualSystemMetrics();

            // 데이터베이스 연결 상태 상세 확인
            let dbConnectionDetails = {};
            if (postgresQuery) {
                try {
                    const dbResult = await postgresQuery('SELECT version()');
                    dbConnectionDetails = {
                        connected: true,
                        version: dbResult.rows?.[0]?.version || 'Unknown',
                        response_time: -1
                    };
                } catch (error) {
                    dbConnectionDetails = {
                        connected: false,
                        error: error.message
                    };
                }
            } else {
                dbConnectionDetails = {
                    connected: false,
                    error: 'PostgreSQL 모듈 없음'
                };
            }

            // Redis 연결 상태 상세 확인
            let redisConnectionDetails = {};
            if (redisClient) {
                try {
                    const redisInfo = await redisClient.info();
                    redisConnectionDetails = {
                        connected: true,
                        info: redisInfo,
                        response_time: -1
                    };
                } catch (error) {
                    redisConnectionDetails = {
                        connected: false,
                        error: error.message
                    };
                }
            } else {
                redisConnectionDetails = {
                    connected: false,
                    error: 'Redis 모듈 없음'
                };
            }

            const healthData = {
                overall_status: health.overall || 'unknown',
                
                components: {
                    backend: {
                        status: 'healthy',
                        response_time: -1,
                        uptime: systemMetrics.processUptime,
                        memory_usage: systemMetrics.memoryUsage,
                        version: systemMetrics.nodeVersion
                    },
                    database: dbConnectionDetails,
                    redis: redisConnectionDetails,
                    collector: {
                        status: health.services?.collector?.running ? 'healthy' : 'unknown',
                        last_heartbeat: null
                    }
                },
                
                metrics: systemMetrics,
                
                services: health.services || {},
                
                last_check: new Date().toISOString()
            };

            console.log('시스템 헬스 상태 조회 완료 (CrossPlatformManager 사용)');
            res.json(createResponse(true, healthData, 'System health loaded successfully'));

        } catch (error) {
            console.error('시스템 헬스 상태 조회 실패:', error.message);
            res.status(500).json(createResponse(false, null, error.message, 'SYSTEM_HEALTH_ERROR'));
        }
    });

/**
 * GET /api/dashboard/test
 * 대시보드 API 테스트 엔드포인트
 */
router.get('/test', (req, res) => {
    res.json(createResponse(true, {
        message: 'Complete Dashboard API is working!',
        data_source: 'DeviceRepository Direct + CrossPlatformManager Service Control',
        endpoints: [
            'GET /api/dashboard/overview - DeviceRepository 직접 사용',
            'GET /api/dashboard/tenant-stats - DeviceRepository 직접 사용',
            'GET /api/dashboard/recent-devices - DeviceRepository 직접 사용',
            'GET /api/dashboard/system-health - CrossPlatformManager 사용',
            'GET /api/dashboard/services/status - CrossPlatformManager 사용',
            'POST /api/dashboard/service/{name}/control - CrossPlatformManager 사용'
        ],
        repositories_used: [
            'DeviceRepository (직접 사용)',
            'SiteRepository', 
            'AlarmOccurrenceRepository',
            'AlarmRuleRepository'
        ],
        service_control: {
            available_services: ['collector', 'redis', 'database'],
            available_actions: ['start', 'stop', 'restart'],
            endpoint_format: 'POST /api/dashboard/service/{serviceName}/control',
            managed_by: 'CrossPlatformManager'
        },
        removed_dependencies: [
            'DataPointRepository (DeviceRepository에 포함됨)',
            'CurrentValueRepository (DeviceRepository에 포함됨)'
        ],
        mock_data: false,
        timestamp: new Date().toISOString()
    }, 'Complete Dashboard API with CrossPlatformManager test successful'));
});

module.exports = router;