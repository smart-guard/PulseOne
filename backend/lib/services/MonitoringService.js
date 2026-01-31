// =============================================================================
// backend/lib/services/MonitoringService.js
// =============================================================================

const BaseService = require('./BaseService');
const os = require('os');
const fs = require('fs');
const { execSync } = require('child_process');
const CrossPlatformManager = require('./crossPlatformManager');

/**
 * Service class for System Monitoring and Health Checks.
 */
class MonitoringService extends BaseService {
    constructor() {
        super(null);
    }

    /**
     * Collect all system metrics.
     */
    async getSystemMetrics() {
        return await this.handleRequest(async () => {
            const [cpuUsage, diskInfo] = await Promise.all([
                this._getCPUUsage(),
                this._getDiskUsage()
            ]);

            const networkUsage = this._getNetworkUsage();
            const processInfo = this._getProcessInfo();

            const totalMemory = os.totalmem();
            const freeMemory = os.freemem();
            const usedMemory = totalMemory - freeMemory;
            const memoryUsagePercent = Math.round((usedMemory / totalMemory) * 100);

            return {
                timestamp: new Date().toISOString(),
                cpu: {
                    usage: cpuUsage,
                    cores: os.cpus().length,
                    model: os.cpus()[0]?.model || 'Unknown'
                },
                memory: {
                    total: Math.round(totalMemory / (1024 * 1024 * 1024)),
                    used: Math.round(usedMemory / (1024 * 1024 * 1024)),
                    free: Math.round(freeMemory / (1024 * 1024 * 1024)),
                    usage: memoryUsagePercent
                },
                disk: diskInfo,
                network: {
                    usage: networkUsage,
                    interfaces: Object.keys(os.networkInterfaces()).length
                },
                system: {
                    platform: os.platform(),
                    arch: os.arch(),
                    hostname: os.hostname(),
                    uptime: Math.round(os.uptime())
                },
                process: processInfo
            };
        }, 'MonitoringService.getSystemMetrics');
    }

    /**
     * Get health status of all managed services.
     */
    async getServiceHealth() {
        return await this.handleRequest(async () => {
            const result = await CrossPlatformManager.getServicesForAPI();

            if (!result.success) {
                throw new Error(result.error || 'Failed to check service health');
            }

            const services = {};
            result.data.forEach(service => {
                services[service.name] = service.status === 'running' ? 'healthy' : 'error';
            });

            // Default fallback logic from original route
            if (!services.backend) services.backend = 'healthy';
            if (!services.database) services.database = 'healthy';
            if (!services.redis) services.redis = services.redis || 'error';
            if (!services.collector) services.collector = services.collector || 'error';

            const healthyCount = Object.values(services).filter(status => status === 'healthy').length;
            const totalCount = Object.keys(services).length;
            const overall = healthyCount === totalCount ? 'healthy' :
                healthyCount > totalCount / 2 ? 'degraded' : 'critical';

            return {
                services,
                overall,
                healthy_count: healthyCount,
                total_count: totalCount,
                last_check: new Date().toISOString(),
                platform: result.platform?.type || 'unknown'
            };
        }, 'MonitoringService.getServiceHealth');
    }

    /**
     * Get database statistics.
     */
    async getDatabaseStats() {
        return await this.handleRequest(async () => {
            const ConfigManager = require('../config/ConfigManager');
            const sqlite3 = require('sqlite3');
            const config = ConfigManager.getInstance();
            const dbPath = config.get('DATABASE_PATH') || './data/db/pulseone.db';

            const stats = await new Promise((resolve, reject) => {
                const db = new sqlite3.Database(dbPath, (err) => {
                    if (err) return reject(err);

                    const queries = {
                        table_count: "SELECT COUNT(*) as count FROM sqlite_master WHERE type='table'",
                        device_count: "SELECT COUNT(*) as count FROM devices",
                        data_point_count: "SELECT COUNT(*) as count FROM data_points",
                        active_alarm_count: "SELECT COUNT(*) as count FROM alarm_occurrences WHERE state='active'",
                        user_count: "SELECT COUNT(*) as count FROM users"
                    };

                    const results = {};
                    const keys = Object.keys(queries);
                    let completed = 0;

                    keys.forEach(key => {
                        db.get(queries[key], (err, row) => {
                            results[key] = row ? row.count : 0;
                            completed++;
                            if (completed === keys.length) {
                                db.close();
                                resolve(results);
                            }
                        });
                    });
                });
            });

            let dbSizeMB = 0;
            try {
                const dbStats = fs.statSync(dbPath);
                dbSizeMB = Math.round(dbStats.size / (1024 * 1024) * 100) / 100;
            } catch (e) { }

            return {
                connection_status: 'connected',
                database_file: dbPath,
                database_size_mb: dbSizeMB,
                tables: stats.table_count,
                devices: stats.device_count,
                data_points: stats.data_point_count,
                active_alarms: stats.active_alarm_count,
                users: stats.user_count,
                last_updated: new Date().toISOString()
            };
        }, 'MonitoringService.getDatabaseStats');
    }

    /**
     * Get performance metrics (Simulated for now).
     */
    async getPerformanceMetrics() {
        return await this.handleRequest(async () => {
            // TODO: 실제 API 및 DB 성능 지표 수집 로직 구현
            // 현재는 시스템 부하에 따른 상대적인 값 제공
            const cpuUsage = await this._getCPUUsage();

            return {
                timestamp: new Date().toISOString(),
                api: {
                    response_time_ms: 10 + Math.floor(cpuUsage / 5), // CPU 부하에 비례
                    throughput_per_second: 0, // 실제 요청 카운터 필요
                    error_rate: 0
                },
                database: {
                    query_time_ms: 5 + Math.floor(cpuUsage / 10),
                    connection_pool_usage: 0 // 실제 커넥션 풀 정보 필요
                },
                cache: {
                    hit_rate: 100,
                    miss_rate: 0
                }
            };
        }, 'MonitoringService.getPerformanceMetrics');
    }

    /**
     * Get system logs.
     */
    async getLogs(level = 'all', limit = 100) {
        return await this.handleRequest(async () => {
            // Simplified placeholder logic
            const logs = [
                { timestamp: new Date().toISOString(), level: 'info', service: 'backend', message: 'API Server running' },
                { timestamp: new Date(Date.now() - 60000).toISOString(), level: 'warn', service: 'redis', message: 'Redis connection attempt...' },
                { timestamp: new Date(Date.now() - 120000).toISOString(), level: 'error', service: 'collector', message: 'Collector service stopped' }
            ];

            const filteredLogs = level === 'all' ? logs : logs.filter(log => log.level === level);
            return filteredLogs.slice(0, limit);
        }, 'MonitoringService.getLogs');
    }

    /**
     * Internal: Calculate CPU Usage
     */
    async _getCPUUsage() {
        return new Promise((resolve) => {
            const startMeasure = os.cpus();
            setTimeout(() => {
                const endMeasure = os.cpus();
                let totalIdle = 0;
                let totalTick = 0;
                for (let i = 0; i < startMeasure.length; i++) {
                    const startCpu = startMeasure[i];
                    const endCpu = endMeasure[i];
                    const startTotal = Object.values(startCpu.times).reduce((acc, time) => acc + time, 0);
                    const endTotal = Object.values(endCpu.times).reduce((acc, time) => acc + time, 0);
                    totalIdle += endCpu.times.idle - startCpu.times.idle;
                    totalTick += endTotal - startTotal;
                }
                const usage = Math.round(100 - (100 * totalIdle / totalTick));
                resolve(Math.max(0, Math.min(100, usage)));
            }, 500); // Reduced delay for responsiveness
        });
    }

    /**
     * Internal: Get Disk Usage
     */
    async _getDiskUsage() {
        try {
            if (process.platform === 'win32') {
                const output = execSync('wmic logicaldisk get size,freespace,caption', { encoding: 'utf8' });
                const lines = output.split('\n').filter(line => line.includes(':'));
                if (lines.length > 0) {
                    const parts = lines[0].trim().split(/\s+/);
                    const freeSpace = parseInt(parts[1]);
                    const totalSpace = parseInt(parts[2]);
                    return {
                        total: Math.round(totalSpace / (1024 ** 3)),
                        used: Math.round((totalSpace - freeSpace) / (1024 ** 3)),
                        free: Math.round(freeSpace / (1024 ** 3)),
                        usage: Math.round(((totalSpace - freeSpace) / totalSpace) * 100)
                    };
                }
            } else {
                const output = execSync('df -h /', { encoding: 'utf8' });
                const lines = output.split('\n');
                if (lines.length > 1) {
                    const parts = lines[1].split(/\s+/);
                    return {
                        total: Math.round(parseFloat(parts[1])),
                        used: Math.round(parseFloat(parts[2])),
                        free: Math.round(parseFloat(parts[3])),
                        usage: parseInt(parts[4])
                    };
                }
            }
        } catch (e) {
            return { total: 0, used: 0, free: 0, usage: 0 };
        }
    }

    /**
     * Internal: Get Network Usage (Estimated)
     */
    _getNetworkUsage() {
        // 인터페이스 트래픽 통계 수집은 OS별로 복잡하므로 
        // 현재는 활성 인터페이스 유무에 따라 기본값 반환
        try {
            const interfaces = os.networkInterfaces();
            const activeCount = Object.keys(interfaces).filter(name =>
                interfaces[name].some(addr => !addr.internal)
            ).length;
            return activeCount > 0 ? 1 : 0;
        } catch (e) {
            return 0;
        }
    }

    /**
     * Internal: Get Process Info
     */
    _getProcessInfo() {
        const memoryUsage = process.memoryUsage();
        return {
            pid: process.pid,
            uptime: Math.round(process.uptime()),
            memory: {
                rss: Math.round(memoryUsage.rss / (1024 * 1024)),
                heapUsed: Math.round(memoryUsage.heapUsed / (1024 * 1024))
            },
            version: process.version
        };
    }
}

module.exports = new MonitoringService();
