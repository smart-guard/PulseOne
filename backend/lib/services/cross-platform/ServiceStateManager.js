// =============================================================================
// backend/lib/services/cross-platform/ServiceStateManager.js
// 서비스 상태 집계 및 헬스체크
// 원본: crossPlatformManager.js L395~L711
// =============================================================================

'use strict';

const path = require('path');
const config = require('../../config/ConfigManager');

class ServiceStateManager {
    /**
     * @param {import('./PlatformContext')} ctx
     * @param {import('./ProcessMonitor')} processMonitor
     */
    constructor(ctx, processMonitor) {
        this.ctx = ctx;
        this.processMonitor = processMonitor;
    }

    // =========================================================================
    // 📊 전체 서비스 상태 집계 (원본 L399~L633)
    // 로컬 프로세스 + Docker 컨테이너 정보를 합산하여 UI용 배열 반환
    // =========================================================================

    async getServicesForAPI() {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', '서비스 상태 조회 시작');

        const processes = await processMonitor.getRunningProcesses();

        // 파일 존재 확인 (병렬)
        const [collectorExists, redisExists] = await Promise.all([
            ctx.fileExists(ctx.paths.collector),
            ctx.fileExists(ctx.paths.redis)
        ]);

        ctx.log('DEBUG', '파일 존재 확인 완료', {
            collectorPath: ctx.paths.collector, collectorExists,
            redisPath: ctx.paths.redis, redisExists
        });

        // ── Collector 서비스 목록 ──────────────────────────────────────────
        const collectorServices = processes.collector.map((proc, index) => ({
            name: proc.collectorId !== undefined ? `collector-${proc.collectorId}` : `collector-${index}`,
            displayName: proc.collectorId !== undefined ? `Collector (ID: ${proc.collectorId})` : 'Data Collector',
            icon: 'fas fa-download',
            description: `C++ 데이터 수집 서비스 (${ctx.platform})`,
            controllable: true,
            status: 'running',
            pid: proc.pid,
            collectorId: proc.collectorId || null,
            platform: ctx.platform,
            executable: path.basename(ctx.paths.collector),
            uptime: ctx.calculateUptime(proc.startTime),
            memoryUsage: proc.memory || 'N/A',
            cpuUsage: proc.cpu || 'N/A',
            executablePath: ctx.paths.collector,
            exists: collectorExists
        }));

        // 실행 중인 Collector 없으면 stopped 항목 하나 기본 추가 (Docker 업데이트 대상)
        if (collectorServices.length === 0) {
            collectorServices.push({
                name: 'collector', displayName: 'Data Collector',
                icon: 'fas fa-download',
                description: `C++ 데이터 수집 서비스 (${ctx.platform})`,
                controllable: true, status: 'stopped', pid: null,
                platform: ctx.platform, executable: path.basename(ctx.paths.collector),
                uptime: 'N/A', memoryUsage: 'N/A', cpuUsage: 'N/A',
                executablePath: ctx.paths.collector, exists: collectorExists
            });
        }

        // ── 기본 서비스 목록 (backend + collector + redis) ─────────────────
        const services = [
            {
                name: 'backend', displayName: 'Backend API',
                icon: 'fas fa-server',
                description: `REST API 서버 (${ctx.platform})`,
                controllable: false,
                status: processes.backend.length > 0 ? 'running' : 'stopped',
                pid: processes.backend[0]?.pid || null,
                platform: ctx.platform,
                executable: ctx.isWindows ? 'node.exe' : 'node',
                uptime: processes.backend[0] ? ctx.calculateUptime(processes.backend[0].startTime) : 'N/A',
                memoryUsage: processes.backend[0]?.memory || 'N/A',
                cpuUsage: processes.backend[0]?.cpu || 'N/A'
            },
            ...collectorServices,
            {
                name: 'redis', displayName: 'Redis Cache',
                icon: 'fas fa-database',
                description: `인메모리 캐시 서버 (${ctx.platform})`,
                controllable: true,
                status: processes.redis.length > 0 ? 'running' : 'stopped',
                pid: processes.redis[0]?.pid || null,
                platform: ctx.platform,
                executable: path.basename(ctx.paths.redis),
                uptime: processes.redis[0] ? ctx.calculateUptime(processes.redis[0].startTime) : 'N/A',
                memoryUsage: processes.redis[0]?.memory || 'N/A',
                cpuUsage: processes.redis[0]?.cpu || 'N/A',
                executablePath: ctx.paths.redis, exists: redisExists,
                port: config.getRedisConfig?.()?.port || 6379
            }
        ];

        // ── Export Gateway 서비스 목록 ────────────────────────────────────
        const exportGatewayExists = await ctx.fileExists(ctx.paths.exportGateway);
        const exportGatewayServices = processes.exportGateway.map((proc, index) => ({
            name: proc.gatewayId !== undefined ? `export-gateway-${proc.gatewayId}` : `export-gateway-${index}`,
            displayName: proc.gatewayId !== undefined ? `Export Gateway (ID: ${proc.gatewayId})` : 'Export Gateway',
            icon: 'fas fa-satellite-dish',
            description: `데이터 내보내기 서비스 (${ctx.platform})`,
            controllable: true, status: 'running', pid: proc.pid,
            gatewayId: proc.gatewayId || null, platform: ctx.platform,
            executable: path.basename(ctx.paths.exportGateway),
            uptime: ctx.calculateUptime(proc.startTime),
            memoryUsage: proc.memory || 'N/A', cpuUsage: proc.cpu || 'N/A',
            executablePath: ctx.paths.exportGateway, exists: exportGatewayExists
        }));

        if (exportGatewayServices.length === 0) {
            exportGatewayServices.push({
                name: 'export-gateway', displayName: 'Export Gateway',
                icon: 'fas fa-satellite-dish',
                description: `데이터 내보내기 서비스 (${ctx.platform})`,
                controllable: true, status: 'stopped', pid: null,
                platform: ctx.platform, executable: path.basename(ctx.paths.exportGateway),
                uptime: 'N/A', memoryUsage: 'N/A', cpuUsage: 'N/A',
                executablePath: ctx.paths.exportGateway, exists: exportGatewayExists
            });
        }
        services.push(...exportGatewayServices);

        // ── Modbus Slave 서비스 목록 ──────────────────────────────────────
        const modbusSlaveExists = await ctx.fileExists(ctx.paths.modbusSlave || '');
        const modbusSlaveProcesses = processes.modbusSlave || [];
        const modbusSlaveServices = modbusSlaveProcesses.map((proc, index) => ({
            name: proc.deviceId !== undefined ? `modbus-slave-${proc.deviceId}` : `modbus-slave-${index}`,
            displayName: proc.deviceId !== undefined ? `Modbus Slave (ID: ${proc.deviceId})` : 'Modbus Slave',
            icon: 'fas fa-network-wired',
            description: `Modbus TCP Slave 서비스 (${ctx.platform})`,
            controllable: true, status: 'running', pid: proc.pid,
            deviceId: proc.deviceId || null, platform: ctx.platform,
            executable: path.basename(ctx.paths.modbusSlave || 'pulseone-modbus-slave'),
            uptime: ctx.calculateUptime(proc.startTime),
            memoryUsage: proc.memory || 'N/A', cpuUsage: proc.cpu || 'N/A',
            executablePath: ctx.paths.modbusSlave, exists: modbusSlaveExists
        }));

        if (modbusSlaveServices.length === 0) {
            modbusSlaveServices.push({
                name: 'modbus-slave', displayName: 'Modbus Slave',
                icon: 'fas fa-network-wired',
                description: `Modbus TCP Slave 서비스 (${ctx.platform})`,
                controllable: true, status: 'stopped', pid: null,
                platform: ctx.platform,
                executable: path.basename(ctx.paths.modbusSlave || 'pulseone-modbus-slave'),
                uptime: 'N/A', memoryUsage: 'N/A', cpuUsage: 'N/A',
                executablePath: ctx.paths.modbusSlave, exists: modbusSlaveExists
            });
        }
        services.push(...modbusSlaveServices);

        // ── Docker 컨테이너 정보로 stopped 서비스 상태 업데이트 ────────────
        if (ctx.isDocker || ctx.isDevelopment) {
            const dockerInfo = processes.dockerContainers || [];

            for (const service of services) {
                if (service.status !== 'stopped') continue;

                const baseName = service.name.split('-')[0].toLowerCase();
                const container = dockerInfo.find(c => {
                    const names = c.Names.map(n => n.replace(/^\//, '').toLowerCase());
                    return names.some(name => {
                        if (baseName === 'export') {
                            return name.includes('export-gateway') || name.includes('win-gateway') || name.includes('gateway');
                        }
                        return name.includes(baseName);
                    });
                });

                if (container) {
                    const isRunning = container.State === 'running' || container.Status.toLowerCase().includes('up');
                    if (isRunning) {
                        service.status = 'running';
                        service.description += ' (Docker Managed)';
                        if (!service.uptime || service.uptime === 'N/A') service.uptime = container.Status;
                        service.containerId = container.Id;
                    }
                }
            }
        }

        // ── 집계 요약 ─────────────────────────────────────────────────────
        const summary = services.reduce((acc, s) => {
            acc.total++;
            if (s.status === 'running') acc.running++;
            else if (s.status === 'stopped') acc.stopped++;
            return acc;
        }, { total: 0, running: 0, stopped: 0, unknown: 0 });

        ctx.log('INFO', '서비스 상태 조회 완료', { summary });

        return {
            success: true,
            data: services,
            summary,
            platform: {
                type: ctx.platform,
                architecture: ctx.architecture,
                deployment: ctx.getDeploymentType(),
                development: ctx.isDevelopment
            },
            paths: ctx.paths,
            timestamp: new Date().toISOString()
        };
    }

    // =========================================================================
    // 🏥 헬스 체크 (원본 L639~L711)
    // 실행파일/디렉토리 존재 + 프로세스 상태 종합 판단
    // =========================================================================

    async performHealthCheck() {
        const { ctx, processMonitor } = this;
        ctx.log('INFO', '헬스체크 시작');

        const health = {
            overall: 'healthy',
            platform: ctx.platform,
            architecture: ctx.architecture,
            development: ctx.isDevelopment,
            services: {},
            system: await ctx.getSystemInfo(),
            checks: {
                collector_executable: await ctx.fileExists(ctx.paths.collector),
                redis_executable: await ctx.fileExists(ctx.paths.redis),
                database_file: await ctx.fileExists(ctx.paths.sqlite),
                config_directory: await ctx.directoryExists(ctx.paths.config),
                logs_directory: await ctx.directoryExists(ctx.paths.logs),
                data_directory: await ctx.directoryExists(ctx.paths.data)
            },
            timestamp: new Date().toISOString()
        };

        try {
            const processes = await processMonitor.getRunningProcesses();

            health.services.backend = {
                running: processes.backend.length > 0,
                pid: processes.backend[0]?.pid || null,
                platform: ctx.platform, essential: true
            };
            health.services.collector = {
                running: processes.collector.length > 0,
                pid: processes.collector[0]?.pid || null,
                platform: ctx.platform, essential: false,
                available: health.checks.collector_executable
            };
            health.services.redis = {
                running: processes.redis.length > 0,
                pid: processes.redis[0]?.pid || null,
                platform: ctx.platform, essential: false,
                available: health.checks.redis_executable
            };

            // 전체 상태 결정
            if (!health.services.backend.running) {
                health.overall = 'critical';
            } else if (!health.services.collector.running && health.checks.collector_executable) {
                health.overall = 'degraded';
            } else if (!health.services.redis.running && health.checks.redis_executable) {
                health.overall = 'degraded';
            }

            ctx.log('INFO', '헬스체크 완료', {
                overall: health.overall,
                services: {
                    backend: health.services.backend.running,
                    collector: health.services.collector.running,
                    redis: health.services.redis.running
                }
            });

        } catch (error) {
            health.overall = 'error';
            health.error = error.message;
            ctx.log('ERROR', '헬스체크 실패', { error: error.message });
        }

        return health;
    }
}

module.exports = ServiceStateManager;
