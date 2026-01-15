// =============================================================================
// backend/lib/services/SystemService.js
// =============================================================================

const BaseService = require('./BaseService');
const os = require('os');
const ConfigManager = require('../config/ConfigManager');

/**
 * Service class for System-wide information and health checks.
 */
class SystemService extends BaseService {
    constructor() {
        super(null);
        this.configManager = ConfigManager.getInstance();
    }

    /**
     * Check status of all configured databases and collectors.
     */
    async getDatabaseStatus() {
        return await this.handleRequest(async () => {
            const databaseType = this.configManager.get('DATABASE_TYPE', 'SQLITE').toUpperCase();
            const results = {};

            switch (databaseType) {
                case 'SQLITE':
                case 'SQLITE3':
                    results.sqlite = await this._checkSQLiteStatus();
                    break;
                case 'POSTGRESQL':
                case 'POSTGRES':
                    results.postgresql = await this._checkPostgreSQLStatus();
                    break;
                default:
                    results.sqlite = await this._checkSQLiteStatus();
            }

            if (this.configManager.getBoolean('REDIS_PRIMARY_ENABLED', false)) {
                results.redis = await this._checkRedisStatus();
            }

            // Collector Health Checks
            const CollectorProxy = require('./CollectorProxyService').getInstance();
            const edgeRepo = require('../database/repositories/RepositoryFactory').getInstance().getEdgeServerRepository();
            const edgeServers = await edgeRepo.findAll();

            results.collectors = await Promise.all(edgeServers.map(async (es) => {
                const health = await CollectorProxy.healthCheck(es.id).catch(() => ({ success: false }));
                return {
                    id: es.id,
                    name: es.server_name,
                    status: health.success ? 'healthy' : 'unhealthy',
                    ip: es.ip_address
                };
            }));

            return results;
        }, 'SystemService.getDatabaseStatus');
    }

    /**
     * Get system configuration (sensitive info masked).
     */
    async getSystemConfig() {
        return await this.handleRequest(async () => {
            return {
                database: {
                    type: this.configManager.get('DATABASE_TYPE', 'SQLITE'),
                    sqlite: {
                        path: this.configManager.get('SQLITE_PATH', './data/db/pulseone.db')
                    }
                },
                server: {
                    port: this.configManager.getNumber('BACKEND_PORT', 3000),
                    env: this.configManager.get('NODE_ENV', 'development')
                }
            };
        }, 'SystemService.getSystemConfig');
    }

    /**
     * Get OS and process information.
     */
    async getSystemInfo() {
        return await this.handleRequest(async () => {
            return {
                node: {
                    version: process.version,
                    uptime: process.uptime()
                },
                os: {
                    platform: os.platform(),
                    hostname: os.hostname(),
                    uptime: os.uptime()
                },
                memory: {
                    total: os.totalmem(),
                    free: os.freemem()
                }
            };
        }, 'SystemService.getSystemInfo');
    }

    // -- Internal Health Checks (Simplified for Service Layer) --

    async _checkSQLiteStatus() {
        try {
            const sqlite3 = require('sqlite3');
            const path = this.configManager.get('SQLITE_DB_PATH', './data/db/pulseone.db');
            return new Promise((resolve) => {
                const db = new sqlite3.Database(path, (err) => {
                    if (err) resolve({ status: 'error', error: err.message });
                    else { db.close(); resolve({ status: 'connected', path }); }
                });
            });
        } catch (e) {
            return { status: 'error', error: e.message };
        }
    }

    async _checkPostgreSQLStatus() {
        try {
            const postgres = require('../connection/postgres');
            const isReady = postgres.isReady && await postgres.isReady();
            return { status: isReady ? 'connected' : 'disconnected' };
        } catch (e) {
            return { status: 'error', error: e.message };
        }
    }

    async _checkRedisStatus() {
        try {
            const redis = require('../connection/redis');
            return { status: redis.isConnected ? 'connected' : 'disconnected' };
        } catch (e) {
            return { status: 'error', error: e.message };
        }
    }
}

module.exports = new SystemService();
