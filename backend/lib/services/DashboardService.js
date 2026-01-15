// =============================================================================
// backend/lib/services/DashboardService.js
// 대시보드 통계 및 시스템 상태 관리 서비스
// =============================================================================

const BaseService = require('./BaseService');
const RepositoryFactory = require('../database/repositories/RepositoryFactory');
const CrossPlatformManager = require('./crossPlatformManager');

class DashboardService extends BaseService {
    constructor() {
        super(null);
    }

    /**
     * RepositoryFactory를 통해 필요한 리포지토리들을 가져옵니다.
     */
    get deviceRepo() {
        return RepositoryFactory.getInstance().getRepository('DeviceRepository');
    }

    get siteRepo() {
        return RepositoryFactory.getInstance().getRepository('SiteRepository');
    }

    get alarmOccurrenceRepo() {
        return RepositoryFactory.getInstance().getRepository('AlarmOccurrenceRepository');
    }

    get alarmRuleRepo() {
        return RepositoryFactory.getInstance().getRepository('AlarmRuleRepository');
    }

    /**
     * 대시보드 전체 시스템 개요 데이터 조회
     */
    async getOverviewData(tenantId) {
        return await this.handleRequest(async () => {
            // 1. 서비스 상태 및 시스템 메트릭 (병렬 조회)
            const [servicesResult, systemInfo, systemMetrics, edgeServers] = await Promise.all([
                CrossPlatformManager.getServicesForAPI(),
                CrossPlatformManager.getSystemInfo(),
                this._getProcessMetrics(),
                this.deviceRepo.knex('edge_servers').where('tenant_id', tenantId).where('is_deleted', false).catch(() => [])
            ]);

            let services = servicesResult.data || [];

            // 1a. Collector 인스턴스 병합 (DB에 등록된 모든 EdgeServer 표시)
            const runningCollectors = services.filter(s => s.name.startsWith('collector'));
            const otherServices = services.filter(s => !s.name.startsWith('collector'));

            const mergedCollectors = edgeServers.map(server => {
                const running = runningCollectors.find(c => c.collectorId === server.id);
                if (running) {
                    return {
                        ...running,
                        displayName: `${server.server_name || 'Collector'}`,
                        description: `관리되는 수집기: ${server.ip_address}:${server.port}`,
                        ip: server.ip_address,
                        collectorId: server.id,
                        exists: true
                    };
                }
                return {
                    name: `collector-${server.id}`,
                    displayName: `${server.server_name || 'Collector'}`,
                    icon: 'fas fa-download',
                    description: `관리되는 수집기: ${server.ip_address}:${server.port}`,
                    ip: server.ip_address,
                    controllable: true,
                    status: 'stopped',
                    pid: null,
                    collectorId: server.id,
                    exists: true,
                    uptime: 'N/A'
                };
            });

            // 1b. 각 콜렉터에 할당된 디바이스 목록 상세 조회 (ID, 이름, 상태 포함)
            const collectorsWithDevices = await Promise.all(mergedCollectors.map(async (collector) => {
                const devs = await this.deviceRepo.findAll(tenantId, {
                    edgeServerId: collector.collectorId,
                    limit: 100 // 요약 목록이므로 상위 100개만
                }).catch(() => []);

                return {
                    ...collector,
                    deviceCount: devs.length,
                    devices: devs.map(d => ({
                        id: d.id,
                        name: d.name,
                        status: d.status.connection,
                        protocol: d.protocol_type,
                        lastSeen: d.status.last_seen
                    }))
                };
            }));

            // 1c. 활성 상태 보정 (Local Process OR DB Heartbeat)
            // 엣지 서버의 경우 로컬 프로세스 감지가 안 될 수 있으므로(Docker/Remote), last_seen을 함께 확인
            const now = new Date();
            const finalizedCollectors = collectorsWithDevices.map(collector => {
                const es = edgeServers.find(s => s.id === collector.collectorId);
                let status = collector.status;

                // 로컬 프로세스 감지가 안 됐더라도 DB last_seen이 최근(2분 이내로 완화)이면 running으로 침
                if (status !== 'running' && es && es.last_seen) {
                    const lastSeen = new Date(es.last_seen);
                    const diff = (now - lastSeen) / 1000;
                    if (diff < 120) { // 2분
                        status = 'running';
                    }
                }

                return { ...collector, status };
            });

            // 1d. 계층형 그룹화 (Site -> Collector -> Device)
            const allSitesResult = await this.siteRepo.findAll(tenantId).catch(() => ({ items: [] }));
            const sites = allSitesResult.items || [];

            const hierarchy = sites.map(site => {
                const siteCollectors = finalizedCollectors.filter(c => {
                    const es = edgeServers.find(es => es.id === c.collectorId);
                    return es && es.site_id === site.id;
                });

                return {
                    id: site.id,
                    name: site.name,
                    code: site.code,
                    collectors: siteCollectors
                };
            });

            // 소속 사이트가 없는 콜렉터들 (Global or Unassigned)
            const unassignedCollectors = finalizedCollectors.filter(c => {
                const es = edgeServers.find(es => es.id === c.collectorId);
                return !es || !es.site_id;
            });

            // 최종 서비스 목록 구성
            const shadowCollectors = runningCollectors
                .filter(rc => rc.collectorId && !edgeServers.find(es => es.id === rc.collectorId))
                .map(sc => ({
                    ...sc,
                    displayName: sc.displayName || '미등록 수집기',
                    description: 'DB에 등록되지 않은 프로세스'
                }));

            // Dashboard UI 호환성을 위해 flat list와 hierarchy를 둘 다 제공
            services = [...otherServices, ...finalizedCollectors, ...shadowCollectors];

            // 2. 디바이스 및 사이트 통계
            const [protocolStats, siteStatsRaw, systemSummary, allDataPoints] = await Promise.all([
                this.deviceRepo.getDeviceStatsByProtocol(tenantId).catch(() => []),
                this.deviceRepo.getDeviceStatsBySite(tenantId).catch(() => []),
                this.deviceRepo.getSystemStatusSummary(tenantId).catch(() => ({})),
                this.deviceRepo.searchDataPoints(tenantId, '').catch(() => [])
            ]);

            // 3. 알람 통계
            const [activeAlarms, todayAlarms, recentAlarms] = await Promise.all([
                this.alarmOccurrenceRepo.findActivePlainSQL(tenantId).catch(() => []),
                this.alarmOccurrenceRepo.findTodayAlarms(tenantId).catch(() => []),
                this.alarmOccurrenceRepo.findRecentAlarms(tenantId, 10).catch(() => [])
            ]);

            // 데이터 가공
            const deviceStats = {
                total: systemSummary.total_devices || 0,
                active: systemSummary.active_devices || 0,
                inactive: systemSummary.inactive_devices || 0,
                connected: systemSummary.connected_devices || 0,
                disconnected: systemSummary.disconnected_devices || 0,
                error: systemSummary.error_devices || 0,
                protocols: protocolStats.reduce((acc, p) => { acc[p.protocol_type] = p.device_count; return acc; }, {}),
                sites_count: sites.length || 0
            };

            const dataPointStats = {
                total: allDataPoints.length,
                active: allDataPoints.filter(dp => dp.is_enabled).length,
                analog: allDataPoints.filter(dp => dp.data_type === 'analog' || dp.data_type === 'REAL').length,
                digital: allDataPoints.filter(dp => dp.data_type === 'digital' || dp.data_type === 'BOOL').length,
                string: allDataPoints.filter(dp => dp.data_type === 'string' || dp.data_type === 'STRING').length
            };

            const overviewData = {
                services: {
                    total: services.length,
                    running: services.filter(s => s.status === 'running').length,
                    stopped: services.filter(s => s.status === 'stopped').length,
                    error: services.filter(s => s.status === 'error').length,
                    details: services
                },
                system_metrics: {
                    ...systemMetrics,
                    platform: systemInfo.platform.type,
                    arch: systemInfo.platform.architecture
                },
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
                },
                hierarchy: hierarchy,
                unassigned_collectors: unassignedCollectors
            };

            return overviewData;
        }, 'DashboardService.getOverviewData');
    }

    /**
     * 테넌트 상세 통계 조회
     */
    async getTenantStats(tenantId) {
        return await this.handleRequest(async () => {
            const [
                protocolStats,
                siteStatsRaw,
                systemSummary,
                recentActive,
                sites,
                alarmStatsRaw
            ] = await Promise.all([
                this.deviceRepo.getDeviceStatsByProtocol(tenantId).catch(() => []),
                this.deviceRepo.getDeviceStatsBySite(tenantId).catch(() => []),
                this.deviceRepo.getSystemStatusSummary(tenantId).catch(() => ({})),
                this.deviceRepo.getRecentActiveDevices(tenantId, 10).catch(() => []),
                this.siteRepo.findAll(tenantId).catch(() => []),
                this.alarmOccurrenceRepo.getStatsByTenant(tenantId).catch(() => null)
            ]);

            const alarmStats = alarmStatsRaw || {
                active: 0,
                total_today: 0,
                total_week: 0,
                total_month: 0,
                by_severity: {},
                by_device: {},
                response_times: {}
            };

            return {
                tenant_id: tenantId,
                tenant_name: `Tenant ${tenantId}`,
                devices: {
                    total: systemSummary.total_devices || 0,
                    active: systemSummary.active_devices || 0,
                    inactive: systemSummary.inactive_devices || 0,
                    connected: systemSummary.connected_devices || 0,
                    disconnected: systemSummary.disconnected_devices || 0,
                    by_protocol: protocolStats.reduce((acc, p) => { acc[p.protocol_type] = p.device_count; return acc; }, {}),
                    by_site: siteStatsRaw.reduce((acc, s) => { acc[s.site_name] = s.device_count; return acc; }, {}),
                    recent_additions: recentActive
                },
                sites: {
                    total: sites.length || 0,
                    active: sites.filter(s => s.is_active).length || 0,
                },
                alarms: alarmStats,
                recent_activity: {
                    recent_devices: recentActive || [],
                    last_updated: new Date().toISOString()
                }
            };
        }, 'DashboardService.getTenantStats');
    }

    /**
     * 서비스 제어 핸들러
     */
    async controlService(serviceName, action) {
        return await this.handleRequest(async () => {
            const lowerName = serviceName.toLowerCase();

            if (lowerName === 'collector' || lowerName.startsWith('collector-')) {
                let collectorId = null;
                if (lowerName.startsWith('collector-')) {
                    const idPart = lowerName.split('-')[1];
                    collectorId = idPart === 'default' ? null : parseInt(idPart);
                }
                return await this._handleCollectorAction(action, collectorId);
            }

            switch (lowerName) {
                case 'redis':
                    return await this._handleRedisAction(action);
                case 'database':
                    return await this._handleDatabaseAction(action);
                default:
                    throw new Error(`알 수 없는 서비스입니다: ${serviceName}`);
            }
        }, 'DashboardService.controlService');
    }

    /**
     * 시스템 헬스 상태 조회
     */
    async getSystemHealth() {
        return await this.handleRequest(async () => {
            const [health, systemInfo, systemMetrics] = await Promise.all([
                CrossPlatformManager.performHealthCheck(),
                CrossPlatformManager.getSystemInfo(),
                this._getProcessMetrics()
            ]);

            return {
                overall_status: health.overall || 'unknown',
                components: {
                    backend: {
                        status: 'healthy',
                        uptime: systemMetrics.processUptime,
                        memory_usage: systemMetrics.memoryUsage,
                        version: systemMetrics.nodeVersion
                    },
                    database: {
                        status: health.services?.database?.status === 'running' ? 'healthy' : 'unhealthy'
                    },
                    redis: {
                        status: health.services?.redis?.status === 'running' ? 'healthy' : 'unhealthy'
                    },
                    collector: {
                        status: health.services?.collector?.status === 'running' ? 'healthy' : 'unknown'
                    }
                },
                metrics: {
                    ...systemMetrics,
                    ...systemInfo
                },
                services: health.services || {},
                last_check: new Date().toISOString()
            };
        }, 'DashboardService.getSystemHealth');
    }

    // ==========================================================================
    // 내부 헬퍼 메소드들
    // ==========================================================================

    async _getProcessMetrics() {
        const memUsage = process.memoryUsage();
        return {
            memoryUsage: Math.floor((memUsage.heapUsed / memUsage.heapTotal) * 100),
            processUptime: Math.floor(process.uptime()),
            nodeVersion: process.version
        };
    }

    async _handleCollectorAction(action, collectorId = null) {
        switch (action) {
            case 'start': return await CrossPlatformManager.startCollector(collectorId);
            case 'stop': return await CrossPlatformManager.stopCollector(collectorId);
            case 'restart': return await CrossPlatformManager.restartCollector(collectorId);
            default: throw new Error(`알 수 없는 액션입니다: ${action}`);
        }
    }

    async _handleRedisAction(action) {
        switch (action) {
            case 'start': return await CrossPlatformManager.startRedis();
            case 'stop': return await CrossPlatformManager.stopRedis();
            case 'restart': return await CrossPlatformManager.restartRedis();
            default: throw new Error(`알 수 없는 액션입니다: ${action}`);
        }
    }

    async _handleDatabaseAction(action) {
        // SQLite는 파일 기반이므로 연결 테스트로 대체
        if (action === 'start' || action === 'restart') {
            try {
                // RepositoryFactory를 통해 DB 연결 확인
                const result = await this.deviceRepo.knex.raw('SELECT 1');
                return { success: true, message: '데이터베이스 연결이 정상입니다.', data: { type: 'sqlite', status: 'connected' } };
            } catch (error) {
                return { success: false, error: `데이터베이스 연결 실패: ${error.message}` };
            }
        }
        return { success: true, message: 'SQLite는 파일 기반 데이터베이스입니다.' };
    }
}

module.exports = new DashboardService();
