// ============================================================================
// backend/routes/dashboard.js
// 대시보드 데이터 API - 통계, 상태, 알람 등 종합 정보
// ============================================================================

const express = require('express');
const router = express.Router();
const DeviceRepository = require('../lib/database/repositories/DeviceRepository');
const SiteRepository = require('../lib/database/repositories/SiteRepository');
const { 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus
} = require('../middleware/tenantIsolation');

// Repository 인스턴스
const deviceRepo = new DeviceRepository();
const siteRepo = new SiteRepository();

// ============================================================================
// 🎯 유틸리티 함수들
// ============================================================================

function createResponse(success, data, message, error_code) {
    const response = {
        success,
        timestamp: new Date().toISOString()
    };
    
    if (success) {
        response.data = data;
        response.message = message || 'Success';
    } else {
        response.error = data;
        response.error_code = error_code || 'INTERNAL_ERROR';
    }
    
    return response;
}

/**
 * 서비스 상태 시뮬레이션 (실제 환경에서는 실제 상태 확인)
 */
function getServiceStatus() {
    const services = [
        {
            name: 'backend',
            displayName: 'Backend API',
            status: 'running',
            icon: 'fas fa-server',
            controllable: false,
            container: 'pulseone-backend-dev',
            description: 'REST API 서버 (필수 서비스)',
            uptime: Math.floor(Math.random() * 100000), // seconds
            memory_usage: Math.floor(Math.random() * 512), // MB
            cpu_usage: Math.floor(Math.random() * 50) // %
        },
        {
            name: 'collector',
            displayName: 'Data Collector',
            status: Math.random() > 0.1 ? 'running' : 'stopped',
            icon: 'fas fa-download',
            controllable: true,
            container: 'pulseone-collector-dev',
            description: 'C++ 데이터 수집 서비스',
            uptime: Math.floor(Math.random() * 50000),
            memory_usage: Math.floor(Math.random() * 256),
            cpu_usage: Math.floor(Math.random() * 30)
        },
        {
            name: 'redis',
            displayName: 'Redis Cache',
            status: Math.random() > 0.05 ? 'running' : 'error',
            icon: 'fas fa-database',
            controllable: true,
            container: 'pulseone-redis',
            description: '실시간 데이터 캐시',
            uptime: Math.floor(Math.random() * 200000),
            memory_usage: Math.floor(Math.random() * 128),
            cpu_usage: Math.floor(Math.random() * 20)
        },
        {
            name: 'database',
            displayName: 'Database',
            status: 'running',
            icon: 'fas fa-database',
            controllable: false,
            container: 'pulseone-database',
            description: '메인 데이터베이스',
            uptime: Math.floor(Math.random() * 300000),
            memory_usage: Math.floor(Math.random() * 1024),
            cpu_usage: Math.floor(Math.random() * 40)
        },
        {
            name: 'rabbitmq',
            displayName: 'Message Queue',
            status: Math.random() > 0.1 ? 'running' : 'stopped',
            icon: 'fas fa-exchange-alt',
            controllable: true,
            container: 'pulseone-rabbitmq',
            description: '메시지 큐 시스템',
            uptime: Math.floor(Math.random() * 150000),
            memory_usage: Math.floor(Math.random() * 256),
            cpu_usage: Math.floor(Math.random() * 25)
        }
    ];

    return services;
}

/**
 * 시스템 메트릭 생성
 */
function generateSystemMetrics() {
    return {
        dataPointsPerSecond: Math.floor(Math.random() * 1000) + 500,
        avgResponseTime: Math.floor(Math.random() * 100) + 50, // ms
        dbQueryTime: Math.floor(Math.random() * 50) + 10, // ms
        cpuUsage: Math.floor(Math.random() * 80) + 10, // %
        memoryUsage: Math.floor(Math.random() * 70) + 20, // %
        diskUsage: Math.floor(Math.random() * 50) + 30, // %
        networkUsage: Math.floor(Math.random() * 100) + 50, // Mbps
        activeConnections: Math.floor(Math.random() * 100) + 10,
        queueSize: Math.floor(Math.random() * 50)
    };
}

/**
 * 최근 알람 시뮬레이션
 */
function generateRecentAlarms() {
    const alarmTypes = [
        { type: 'error', message: '디바이스 연결 실패', icon: 'fas fa-exclamation-triangle' },
        { type: 'warning', message: '온도 임계값 초과', icon: 'fas fa-thermometer-three-quarters' },
        { type: 'info', message: '시스템 백업 완료', icon: 'fas fa-info-circle' },
        { type: 'error', message: '네트워크 연결 불안정', icon: 'fas fa-wifi' },
        { type: 'warning', message: '디스크 용량 부족', icon: 'fas fa-hdd' }
    ];

    return Array.from({ length: Math.floor(Math.random() * 5) + 1 }, (_, i) => {
        const alarm = alarmTypes[Math.floor(Math.random() * alarmTypes.length)];
        return {
            id: `alarm_${Date.now()}_${i}`,
            type: alarm.type,
            message: alarm.message,
            icon: alarm.icon,
            timestamp: new Date(Date.now() - Math.random() * 3600000), // 1시간 이내
            device_id: Math.floor(Math.random() * 10) + 1,
            acknowledged: Math.random() > 0.7
        };
    });
}

// ============================================================================
// 🌐 대시보드 API 엔드포인트들
// ============================================================================

/**
 * GET /api/dashboard/overview
 * 대시보드 메인 개요 데이터
 */
router.get('/overview', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            const { tenantId, isSystemAdmin } = req;

            // 서비스 상태
            const services = getServiceStatus();
            
            // 시스템 메트릭
            const systemMetrics = generateSystemMetrics();
            
            // 테넌트 통계 (실제 데이터)
            const deviceStats = await deviceRepo.getStatsByTenant(tenantId);
            
            // 최근 알람
            const recentAlarms = generateRecentAlarms();

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
                
                device_summary: {
                    total_devices: deviceStats?.total_devices || 0,
                    connected_devices: deviceStats?.connected_devices || 0,
                    disconnected_devices: deviceStats?.disconnected_devices || 0,
                    error_devices: deviceStats?.error_devices || 0,
                    protocols_count: deviceStats?.protocols_count || 0,
                    sites_count: deviceStats?.sites_count || 0
                },
                
                alarms: {
                    total: recentAlarms.length,
                    unacknowledged: recentAlarms.filter(a => !a.acknowledged).length,
                    critical: recentAlarms.filter(a => a.type === 'error').length,
                    warnings: recentAlarms.filter(a => a.type === 'warning').length,
                    recent_alarms: recentAlarms.slice(0, 5) // 최근 5개만
                },
                
                health_status: {
                    overall: services.filter(s => s.status === 'running').length >= services.length * 0.8 ? 'healthy' : 'degraded',
                    database: 'healthy',
                    network: 'healthy',
                    storage: systemMetrics.diskUsage < 80 ? 'healthy' : 'warning'
                },
                
                last_updated: new Date().toISOString()
            };

            res.json(createResponse(true, overviewData, 'Dashboard overview retrieved successfully'));

        } catch (error) {
            console.error('Get dashboard overview error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'DASHBOARD_OVERVIEW_ERROR'));
        }
    }
);

/**
 * GET /api/dashboard/tenant-stats
 * 테넌트별 상세 통계
 */
router.get('/tenant-stats', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            const { tenantId, isSystemAdmin } = req;

            // 디바이스 통계
            const deviceStats = await deviceRepo.getStatsByTenant(tenantId);
            
            // 프로토콜별 통계
            const protocolStats = await deviceRepo.getStatsByProtocol(tenantId);
            
            // 사이트별 통계
            const siteStats = await deviceRepo.getStatsBySite(tenantId);

            // 시간별 데이터 포인트 수 (시뮬레이션)
            const hourlyDataPoints = Array.from({ length: 24 }, (_, i) => ({
                hour: i,
                data_points: Math.floor(Math.random() * 10000) + 5000,
                errors: Math.floor(Math.random() * 100)
            }));

            const tenantStats = {
                device_statistics: deviceStats,
                protocol_distribution: protocolStats,
                site_distribution: siteStats,
                data_collection: {
                    total_data_points_today: hourlyDataPoints.reduce((sum, h) => sum + h.data_points, 0),
                    error_rate: (hourlyDataPoints.reduce((sum, h) => sum + h.errors, 0) / 
                               hourlyDataPoints.reduce((sum, h) => sum + h.data_points, 0) * 100).toFixed(2),
                    hourly_breakdown: hourlyDataPoints
                },
                performance: {
                    avg_scan_time: Math.floor(Math.random() * 1000) + 500, // ms
                    success_rate: (95 + Math.random() * 4).toFixed(1), // %
                    peak_concurrent_devices: Math.floor(Math.random() * 50) + 20
                }
            };

            res.json(createResponse(true, tenantStats, 'Tenant statistics retrieved successfully'));

        } catch (error) {
            console.error('Get tenant stats error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'TENANT_STATS_ERROR'));
        }
    }
);

/**
 * GET /api/dashboard/recent-devices
 * 최근 연결된 디바이스 목록
 */
router.get('/recent-devices', 
    authenticateToken, 
    tenantIsolation, 
    validateTenantStatus,
    async (req, res) => {
        try {
            const { tenantId, isSystemAdmin } = req;
            const { limit = 10 } = req.query;

            // 최근 디바이스 조회 (last_seen 기준)
            const recentDevices = await deviceRepo.findWithPagination(
                {}, 
                isSystemAdmin ? null : tenantId, 
                1, 
                parseInt(limit),
                'last_seen',
                'DESC'
            );

            // 각 디바이스에 추가 정보 포함
            const enrichedDevices = recentDevices.devices.map(device => ({
                ...device,
                data_points_count: Math.floor(Math.random() * 50) + 5, // 시뮬레이션
                last_alarm: Math.random() > 0.7 ? {
                    type: 'warning',
                    message: '온도 상승 감지',
                    timestamp: new Date(Date.now() - Math.random() * 86400000) // 24시간 이내
                } : null,
                uptime_percentage: (95 + Math.random() * 4).toFixed(1)
            }));

            res.json(createResponse(true, {
                recent_devices: enrichedDevices,
                total_count: recentDevices.pagination.total_items
            }, 'Recent devices retrieved successfully'));

        } catch (error) {
            console.error('Get recent devices error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'RECENT_DEVICES_ERROR'));
        }
    }
);

/**
 * GET /api/dashboard/system-health
 * 시스템 헬스체크 상세 정보
 */
router.get('/system-health', 
    authenticateToken, 
    tenantIsolation, 
    async (req, res) => {
        try {
            // 각 서비스의 헬스체크
            const services = getServiceStatus();
            
            // 데이터베이스 헬스체크
            const dbHealth = await deviceRepo.healthCheck();
            
            // 시스템 리소스
            const systemResources = {
                cpu: {
                    usage_percent: Math.floor(Math.random() * 80) + 10,
                    load_average: [
                        +(Math.random() * 2).toFixed(2),
                        +(Math.random() * 2).toFixed(2),
                        +(Math.random() * 2).toFixed(2)
                    ],
                    cores: 8
                },
                memory: {
                    total_mb: 16384,
                    used_mb: Math.floor(Math.random() * 12000) + 4000,
                    available_mb: 16384 - (Math.floor(Math.random() * 12000) + 4000),
                    usage_percent: Math.floor(Math.random() * 75) + 15
                },
                disk: {
                    total_gb: 500,
                    used_gb: Math.floor(Math.random() * 300) + 100,
                    available_gb: 500 - (Math.floor(Math.random() * 300) + 100),
                    usage_percent: Math.floor(Math.random() * 60) + 20
                },
                network: {
                    bytes_in: Math.floor(Math.random() * 1000000),
                    bytes_out: Math.floor(Math.random() * 1000000),
                    packets_in: Math.floor(Math.random() * 10000),
                    packets_out: Math.floor(Math.random() * 10000),
                    errors: Math.floor(Math.random() * 10)
                }
            };

            // 전체 헬스 점수 계산
            const healthScore = calculateHealthScore(services, systemResources);

            const healthData = {
                overall_health: {
                    status: healthScore > 80 ? 'healthy' : healthScore > 60 ? 'warning' : 'critical',
                    score: healthScore,
                    last_check: new Date().toISOString()
                },
                services: services.map(service => ({
                    ...service,
                    health_score: service.status === 'running' ? 100 : 0
                })),
                system_resources: systemResources,
                database: {
                    status: dbHealth.status,
                    connection_pool: {
                        active: Math.floor(Math.random() * 10) + 1,
                        idle: Math.floor(Math.random() * 5) + 1,
                        max: 20
                    },
                    query_performance: {
                        avg_query_time: Math.floor(Math.random() * 50) + 10,
                        slow_queries: Math.floor(Math.random() * 5),
                        total_queries: Math.floor(Math.random() * 10000) + 5000
                    }
                },
                recommendations: generateHealthRecommendations(services, systemResources)
            };

            res.json(createResponse(true, healthData, 'System health retrieved successfully'));

        } catch (error) {
            console.error('Get system health error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'SYSTEM_HEALTH_ERROR'));
        }
    }
);

/**
 * POST /api/dashboard/service/:name/control
 * 서비스 제어 (시작/중지/재시작)
 */
router.post('/service/:name/control', 
    authenticateToken, 
    tenantIsolation, 
    async (req, res) => {
        try {
            const { name } = req.params;
            const { action } = req.body; // start, stop, restart

            // 권한 확인 (시스템 관리자만)
            if (req.user?.role !== 'system_admin') {
                return res.status(403).json(
                    createResponse(false, 'Insufficient permissions', null, 'INSUFFICIENT_PERMISSIONS')
                );
            }

            const validActions = ['start', 'stop', 'restart'];
            if (!validActions.includes(action)) {
                return res.status(400).json(
                    createResponse(false, `Invalid action: ${action}`, null, 'INVALID_ACTION')
                );
            }

            // 서비스 제어 시뮬레이션 (실제 환경에서는 실제 제어 로직)
            const controlResult = {
                service: name,
                action: action,
                success: Math.random() > 0.1, // 90% 성공률
                message: `Service ${name} ${action} completed`,
                timestamp: new Date().toISOString()
            };

            if (!controlResult.success) {
                controlResult.message = `Failed to ${action} service ${name}`;
                return res.status(500).json(
                    createResponse(false, controlResult.message, null, 'SERVICE_CONTROL_FAILED')
                );
            }

            res.json(createResponse(true, controlResult, `Service ${action} completed successfully`));

        } catch (error) {
            console.error('Service control error:', error);
            res.status(500).json(createResponse(false, error.message, null, 'SERVICE_CONTROL_ERROR'));
        }
    }
);

// ============================================================================
// 🧮 헬퍼 함수들
// ============================================================================

/**
 * 헬스 점수 계산
 */
function calculateHealthScore(services, systemResources) {
    let score = 100;
    
    // 서비스 상태 (50점)
    const runningServices = services.filter(s => s.status === 'running').length;
    const serviceScore = (runningServices / services.length) * 50;
    
    // CPU 사용률 (20점)
    const cpuScore = Math.max(0, (100 - systemResources.cpu.usage_percent) / 100 * 20);
    
    // 메모리 사용률 (20점)
    const memoryScore = Math.max(0, (100 - systemResources.memory.usage_percent) / 100 * 20);
    
    // 디스크 사용률 (10점)
    const diskScore = Math.max(0, (100 - systemResources.disk.usage_percent) / 100 * 10);
    
    score = serviceScore + cpuScore + memoryScore + diskScore;
    return Math.round(score);
}

/**
 * 헬스 개선 권장사항 생성
 */
function generateHealthRecommendations(services, systemResources) {
    const recommendations = [];
    
    // 서비스 상태 확인
    const stoppedServices = services.filter(s => s.status === 'stopped');
    if (stoppedServices.length > 0) {
        recommendations.push({
            type: 'service',
            priority: 'high',
            message: `${stoppedServices.length}개 서비스가 중지되어 있습니다`,
            action: '중지된 서비스를 시작하세요'
        });
    }
    
    // 리소스 사용량 확인
    if (systemResources.cpu.usage_percent > 80) {
        recommendations.push({
            type: 'performance',
            priority: 'medium',
            message: 'CPU 사용률이 높습니다',
            action: '불필요한 프로세스를 종료하거나 하드웨어 업그레이드를 고려하세요'
        });
    }
    
    if (systemResources.memory.usage_percent > 85) {
        recommendations.push({
            type: 'performance',
            priority: 'medium',
            message: '메모리 사용률이 높습니다',
            action: '메모리 누수를 확인하거나 RAM 확장을 고려하세요'
        });
    }
    
    if (systemResources.disk.usage_percent > 90) {
        recommendations.push({
            type: 'storage',
            priority: 'high',
            message: '디스크 공간이 부족합니다',
            action: '불필요한 파일을 삭제하거나 디스크 공간을 확장하세요'
        });
    }
    
    return recommendations;
}

module.exports = router;