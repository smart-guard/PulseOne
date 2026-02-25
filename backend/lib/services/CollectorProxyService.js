// =============================================================================
// backend/lib/services/CollectorProxyService.js (강화된 버전)
// HTTP 통신 안정성 95% 달성을 위한 완전 개선
// =============================================================================

const axios = require('axios');
const http = require('http');
const ConfigManager = require('../config/ConfigManager');

class CircuitBreaker {
    constructor(options = {}) {
        this.failureThreshold = options.failureThreshold || 5;
        this.recoveryTimeout = options.recoveryTimeout || 30000; // 30초
        this.monitoringPeriod = options.monitoringPeriod || 10000; // 10초

        this.failureCount = 0;
        this.state = 'CLOSED'; // CLOSED, OPEN, HALF_OPEN
        this.nextAttempt = Date.now();
        this.lastFailureTime = null;
    }

    async execute(operation, fallback = null) {
        if (this.state === 'OPEN') {
            if (Date.now() < this.nextAttempt) {
                if (fallback) return fallback();
                throw new Error('Circuit breaker is OPEN');
            }
            this.state = 'HALF_OPEN';
        }

        try {
            const result = await operation();
            this.onSuccess();
            return result;
        } catch (error) {
            this.onFailure();
            if (fallback) return fallback();
            throw error;
        }
    }

    onSuccess() {
        this.failureCount = 0;
        this.state = 'CLOSED';
    }

    onFailure() {
        this.failureCount++;
        this.lastFailureTime = Date.now();

        if (this.failureCount >= this.failureThreshold) {
            this.state = 'OPEN';
            this.nextAttempt = Date.now() + this.recoveryTimeout;
            console.log(`🔴 [CircuitBreaker] Opened. Next attempt at ${new Date(this.nextAttempt).toISOString()}`);
        }
    }

    reset() {
        this.failureCount = 0;
        this.state = 'CLOSED';
        this.nextAttempt = Date.now();
        console.log('🟢 [CircuitBreaker] Reset to CLOSED state');
    }

    getState() {
        return {
            state: this.state,
            failureCount: this.failureCount,
            nextAttempt: this.nextAttempt,
            lastFailure: this.lastFailureTime
        };
    }
}

class ConnectionPool {
    constructor(options = {}) {
        this.maxSockets = options.maxSockets || 50;
        this.keepAlive = options.keepAlive !== false;
        this.keepAliveMsecs = options.keepAliveMsecs || 1000;
        this.maxFreeSockets = options.maxFreeSockets || 10;

        // HTTP Agent 설정 (https 대신 http)
        this.httpAgent = new http.Agent({  // ← 여기 수정
            keepAlive: this.keepAlive,
            keepAliveMsecs: this.keepAliveMsecs,
            maxSockets: this.maxSockets,
            maxFreeSockets: this.maxFreeSockets,
            timeout: 5000
            // freeSocketTimeout는 http.Agent에서 지원되지 않으므로 제거
        });
    }

    getAgent() {
        return this.httpAgent;
    }

    getStats() {
        const sockets = this.httpAgent.sockets || {};
        const freeSockets = this.httpAgent.freeSockets || {};

        return {
            totalSockets: Object.values(sockets).reduce((sum, arr) => sum + arr.length, 0),
            freeSockets: Object.values(freeSockets).reduce((sum, arr) => sum + arr.length, 0),
            maxSockets: this.maxSockets,
            keepAlive: this.keepAlive
        };
    }

    destroy() {
        this.httpAgent.destroy();
    }
}

class CollectorClient {
    constructor(id, host, port, options = {}) {
        this.id = id;
        this.host = host;
        this.port = port;
        this.config = options;
        this.isHealthy = false;
        this.lastHealthCheck = null;
        this.connectionAttempts = 0;
        this.lastConnectionTime = null;
        this.metrics = [];

        this.setupConnectionPool();
        this.setupCircuitBreaker();
        this.setupHttpClient();
        this.setupHealthMonitoring();

        console.log(`🔌 CollectorClient [${id}] initialized: ${host}:${port}`);
    }

    setupConnectionPool() {
        this.connectionPool = new ConnectionPool({
            maxSockets: this.config.connectionPoolSize || 20,
            keepAlive: this.config.keepAlive !== false,
            keepAliveMsecs: 1000,
            maxFreeSockets: Math.max(2, Math.floor((this.config.connectionPoolSize || 20) / 5))
        });
    }

    setupCircuitBreaker() {
        this.circuitBreaker = new CircuitBreaker({
            failureThreshold: this.config.circuitBreakerThreshold || 5,
            recoveryTimeout: this.config.circuitBreakerTimeout || 30000,
            monitoringPeriod: 10000
        });
    }

    setupHttpClient() {
        this.httpClient = axios.create({
            baseURL: `http://${this.host}:${this.port}`,
            timeout: this.config.timeout || 3000,
            headers: {
                'Content-Type': 'application/json',
                'User-Agent': 'PulseOne-Backend/2.1.0-Enhanced',
                'Connection': 'keep-alive'
            },
            httpAgent: this.connectionPool.getAgent(),
            validateStatus: (status) => status >= 200 && status < 300
        });

        this.httpClient.interceptors.request.use(config => {
            config.metadata = { startTime: Date.now() };
            return config;
        });

        this.httpClient.interceptors.response.use(
            (response) => {
                const duration = Date.now() - response.config.metadata.startTime;
                this.isHealthy = true;
                this.lastHealthCheck = new Date();
                this.lastConnectionTime = new Date();
                this.recordMetrics('success', response.status, duration);
                return response;
            },
            (error) => {
                const duration = error.config?.metadata ? Date.now() - error.config.metadata.startTime : 0;
                this.handleApiError(error);
                this.recordMetrics('error', error.response?.status || 0, duration, error.code);
                return Promise.reject(error);
            }
        );
    }

    setupHealthMonitoring() {
        this.healthCheckInterval = setInterval(async () => {
            try { await this.healthCheck(); } catch (e) { }
        }, this.config.healthCheckInterval || 30000);
    }

    recordMetrics(type, statusCode, duration, errorCode = null) {
        this.metrics.push({ timestamp: new Date(), type, statusCode, duration, errorCode });
        if (this.metrics.length > 100) this.metrics.shift();
    }

    handleApiError(error) {
        this.isHealthy = false;
        this.connectionAttempts++;
        console.warn(`⚠️ CollectorClient [${this.id}] Error: ${error.message} (${this.host}:${this.port})`);
    }

    async healthCheck() {
        return await this.circuitBreaker.execute(async () => {
            const response = await this.httpClient.get('/api/health');
            this.isHealthy = true;
            this.lastHealthCheck = new Date();
            this.connectionAttempts = 0;
            return { success: true, data: response.data };
        }, () => ({ success: false, state: 'circuit_open' }));
    }

    async safeRequest(requestFn, retryCount = 1) {
        let lastError;
        for (let i = 0; i <= retryCount; i++) {
            try { return await requestFn(); }
            catch (e) {
                lastError = e;
                if (i < retryCount) await new Promise(r => setTimeout(r, 1000 * Math.pow(2, i)));
            }
        }
        throw lastError;
    }

    // Proxy Methods
    async startDevice(deviceId, options = {}) {
        return await this.circuitBreaker.execute(async () => {
            const resp = await this.safeRequest(() => this.httpClient.post(`/api/devices/${deviceId}/worker/start`, options));
            return { success: true, data: resp.data, deviceId };
        });
    }

    async stopDevice(deviceId, options = {}) {
        return await this.circuitBreaker.execute(async () => {
            const resp = await this.safeRequest(() => this.httpClient.post(`/api/devices/${deviceId}/worker/stop`, options));
            return { success: true, data: resp.data, deviceId };
        });
    }

    async restartDevice(deviceId, options = {}) {
        return await this.circuitBreaker.execute(async () => {
            const resp = await this.safeRequest(() => this.httpClient.post(`/api/devices/${deviceId}/worker/restart`, options));
            return { success: true, data: resp.data, deviceId };
        });
    }

    async getDeviceStatus(deviceId) {
        return await this.circuitBreaker.execute(async () => {
            const resp = await this.safeRequest(() => this.httpClient.get(`/api/devices/${deviceId}/status`));
            return { success: true, data: resp.data, deviceId };
        });
    }

    async getCurrentData(deviceId, pointIds = []) {
        return await this.circuitBreaker.execute(async () => {
            const params = pointIds.length > 0 ? { point_ids: pointIds.join(',') } : {};
            const resp = await this.safeRequest(() => this.httpClient.get(`/api/devices/${deviceId}/data/current`, { params }));
            return { success: true, data: resp.data, deviceId };
        });
    }

    async getWorkerStatus() {
        return await this.circuitBreaker.execute(async () => {
            // 상태 요약 조회는 재시도 없이 1.5초 이내에 응답해야 함
            const resp = await this.safeRequest(
                () => this.httpClient.get('/api/workers/status', { timeout: 1500 }),
                0
            );
            return { success: true, data: resp.data };
        });
    }

    async getSystemStatistics() {
        return await this.circuitBreaker.execute(async () => {
            const resp = await this.safeRequest(() => this.httpClient.get('/api/statistics'));
            return { success: true, data: resp.data };
        });
    }

    async reloadDeviceConfig(deviceId) {
        return await this.safeRequest(() => this.httpClient.post(`/api/devices/${deviceId}/config/reload`));
    }

    async reloadAllConfigs() {
        return await this.safeRequest(() => this.httpClient.post('/api/config/reload'));
    }

    async notifyConfigChange(changeType, entityId, changes = {}) {
        return await this.safeRequest(() => this.httpClient.post('/api/config/notify-change', {
            type: changeType,
            entity_id: entityId,
            changes,
            timestamp: new Date().toISOString()
        }));
    }

    async syncDeviceSettings(deviceId, settings) {
        // 설정 동기화 시도시 회로 차단기가 열려있다면, 절반 개방(HALF_OPEN) 상태로 전환하여 재시도 허용
        if (this.circuitBreaker.state === 'OPEN') {
            console.log(`🔄 [CollectorClient] Attempting sync while CB is OPEN - forcing reset check for device ${deviceId}`);
            // 여기서 reset을 하지 않고 nextAttempt만 앞당겨서 execute가 HALF_OPEN으로 시도하게 함
            this.circuitBreaker.nextAttempt = Date.now();
        }

        return await this.circuitBreaker.execute(async () => {
            const resp = await this.safeRequest(() => this.httpClient.put(`/api/devices/${deviceId}/config`, settings));
            return { success: true, data: resp.data, deviceId };
        });
    }

    async scanNetwork(options = {}) {
        return await this.circuitBreaker.execute(async () => {
            const resp = await this.safeRequest(() => this.httpClient.post('/api/network/scan', options));
            return { success: true, data: resp.data };
        });
    }

    // Control APIs
    async controlDigitalOutput(deviceId, outputId, state, options = {}) {
        return await this.safeRequest(() => this.httpClient.post(`/api/devices/${deviceId}/digital/${outputId}/control`, {
            state: Boolean(state),
            duration: options.duration,
            force: options.force || false
        }));
    }

    async controlAnalogOutput(deviceId, outputId, value, options = {}) {
        return await this.safeRequest(() => this.httpClient.post(`/api/devices/${deviceId}/analog/${outputId}/control`, {
            value: Number(value),
            unit: options.unit,
            ramp_time: options.rampTime
        }));
    }

    async controlPump(deviceId, pumpId, enable, options = {}) {
        return await this.safeRequest(() => this.httpClient.post(`/api/devices/${deviceId}/pump/${pumpId}/control`, {
            enable: Boolean(enable),
            speed: options.speed || 100,
            duration: options.duration
        }));
    }

    shutdown() {
        if (this.healthCheckInterval) clearInterval(this.healthCheckInterval);
        this.connectionPool.destroy();
    }
}

class CollectorProxyService {
    constructor() {
        this.config = ConfigManager.getInstance();
        this.clients = new Map();
        this.setupDefaultClient();
        console.log('🏭 CollectorProxyService (Multi-Instance) initialized');
    }

    setupDefaultClient() {
        const host = this.config.get('COLLECTOR_HOST', 'localhost');
        const port = this.config.getNumber('COLLECTOR_API_PORT', 8080);

        const options = {
            timeout: this.config.getNumber('COLLECTOR_TIMEOUT_MS', 5000),
            connectionPoolSize: this.config.getNumber('COLLECTOR_POOL_SIZE', 20),
            healthCheckInterval: this.config.getNumber('COLLECTOR_HEALTH_INTERVAL', 30000),
            circuitBreakerThreshold: this.config.getNumber('COLLECTOR_CB_THRESHOLD', 5),
            circuitBreakerTimeout: this.config.getNumber('COLLECTOR_CB_TIMEOUT', 30000)
        };

        this.defaultClient = new CollectorClient('default', host, port, options);
        this.clients.set(0, this.defaultClient); // ID 0 is default
    }

    async getClient(edgeServerId = null) {
        const id = edgeServerId || 0;
        if (this.clients.has(id)) return this.clients.get(id);

        try {
            const repo = RepositoryFactory.getInstance().getEdgeServerRepository();
            const server = await repo.findById(id);
            if (!server) return this.defaultClient;

            // Collector 자기등록에 의해 DB에 항상 올바른 IP가 저장됨
            // Docker: "collector", Windows 동일PC: "127.0.0.1", 분리배포: "실제IP"
            let host = server.ip_address || this.config.get('COLLECTOR_HOST', 'localhost');
            const port = server.port || this.config.getNumber('COLLECTOR_API_PORT', 8501);

            // Docker 서비스명 감지: 점이 없고 'localhost'가 아닌 경우
            // (예: "collector", "mqtt-gateway" 등 Docker 내부 서비스명)
            // Windows/Linux 네이티브 환경에서는 resolve 안 되므로 127.0.0.1로 fallback
            if (host && !host.includes('.') && host !== 'localhost') {
                const envOverride = this.config.get('COLLECTOR_HOST', '');
                if (envOverride && envOverride !== host) {
                    host = envOverride;
                } else {
                    console.warn(`🔄 [CollectorProxy] '${host}' looks like a Docker service name - falling back to 127.0.0.1`);
                    host = '127.0.0.1';
                }
            }

            console.log(`🔌 [CollectorProxy] Connecting to edge_server[${id}]: ${host}:${port}`);

            const client = new CollectorClient(
                server.id,
                host,
                port,
                { ...this.defaultClient.config }
            );
            this.clients.set(id, client);
            return client;
        } catch (e) {
            console.error(`Failed to create client for edge server ${id}:`, e.message);
            return this.defaultClient;
        }
    }

    // Dispatcher Methods
    async startDevice(deviceId, options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.startDevice(deviceId, options);
    }

    async stopDevice(deviceId, options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.stopDevice(deviceId, options);
    }

    async restartDevice(deviceId, options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.restartDevice(deviceId, options);
    }

    async getDeviceStatus(deviceId, options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.getDeviceStatus(deviceId);
    }

    async getCurrentData(deviceId, pointIds = [], options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.getCurrentData(deviceId, pointIds);
    }

    async getWorkerStatus(edgeServerId = null) {
        const client = await this.getClient(edgeServerId);
        return await client.getWorkerStatus();
    }

    async getSystemStatistics(edgeServerId = null) {
        const client = await this.getClient(edgeServerId);
        return await client.getSystemStatistics();
    }

    async healthCheck(edgeServerId = null) {
        const client = await this.getClient(edgeServerId);
        return await client.healthCheck();
    }

    isCollectorHealthy(edgeServerId = null) {
        const id = edgeServerId || 0;
        return this.clients.has(id) ? this.clients.get(id).isHealthy : false;
    }

    getLastHealthCheck(edgeServerId = null) {
        const id = edgeServerId || 0;
        return this.clients.has(id) ? this.clients.get(id).lastHealthCheck : null;
    }

    getCollectorConfig(edgeServerId = null) {
        const id = edgeServerId || 0;
        const client = this.clients.get(id) || this.defaultClient;
        return {
            host: client.host,
            port: client.port,
            ...client.config
        };
    }

    async syncDeviceSettings(deviceId, settings, options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.syncDeviceSettings(deviceId, settings);
    }

    async reloadDeviceConfig(deviceId, options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.reloadDeviceConfig(deviceId);
    }

    async scanNetwork(options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.scanNetwork({
            protocol: options.protocol || 'BACNET',
            range: options.range,
            timeout: options.timeout
        });
    }

    async reloadAllConfigs(edgeServerId = null) {
        if (edgeServerId === 'all') {
            const results = await Promise.allSettled(
                Array.from(this.clients.values()).map(client => client.reloadAllConfigs())
            );
            return { success: true, results };
        }
        const client = await this.getClient(edgeServerId);
        return await client.reloadAllConfigs();
    }

    async notifyConfigChange(changeType, entityId, changes = {}, edgeServerId = null) {
        if (edgeServerId === 'all') {
            const results = await Promise.allSettled(
                Array.from(this.clients.values()).map(client => client.notifyConfigChange(changeType, entityId, changes))
            );
            return { success: true, results };
        }
        const client = await this.getClient(edgeServerId);
        return await client.notifyConfigChange(changeType, entityId, changes);
    }

    async controlDigitalOutput(deviceId, outputId, state, options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.controlDigitalOutput(deviceId, outputId, state, options);
    }

    async controlAnalogOutput(deviceId, outputId, value, options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.controlAnalogOutput(deviceId, outputId, value, options);
    }

    async controlPump(deviceId, pumpId, enable, options = {}) {
        const client = await this.getClient(options.edgeServerId);
        return await client.controlPump(deviceId, pumpId, enable, options);
    }

    async batchDeviceControl(operations) {
        // Group operations by edgeServerId
        const groups = operations.reduce((acc, op) => {
            const id = op.options?.edgeServerId || 0;
            if (!acc[id]) acc[id] = [];
            acc[id].push(op);
            return acc;
        }, {});

        const results = await Promise.allSettled(
            Object.entries(groups).map(async ([serverId, ops]) => {
                const client = await this.getClient(parseInt(serverId));
                // For simplicity, we just execute them sequentially per client or 
                // we could implement batchDeviceControl in CollectorClient too
                // Here we'll just mock it or assume simple sequential for now
                const batchResults = [];
                for (const op of ops) {
                    try {
                        let res;
                        switch (op.action) {
                            case 'start': res = await client.startDevice(op.deviceId, op.options); break;
                            case 'stop': res = await client.stopDevice(op.deviceId, op.options); break;
                            case 'restart': res = await client.restartDevice(op.deviceId, op.options); break;
                            case 'status': res = await client.getDeviceStatus(op.deviceId); break;
                            default: res = { success: false, error: 'Unknown action' };
                        }
                        batchResults.push({ ...op, ...res });
                    } catch (e) {
                        batchResults.push({ ...op, success: false, error: e.message });
                    }
                }
                return batchResults;
            })
        );

        const flatResults = results.flatMap(r => r.status === 'fulfilled' ? r.value : []);
        return { success: true, results: flatResults };
    }

    /**
     * 모든 연결된 Collector에 로그레벨 변경 전파
     * @param {string} level - 'DEBUG' | 'INFO' | 'WARN' | 'ERROR' | 'FATAL' | 'TRACE'
     * @param {number|null} edgeServerId - null이면 전체 전파
     */
    async setLogLevel(level, edgeServerId = null) {
        const validLevels = ['TRACE', 'DEBUG', 'INFO', 'WARN', 'ERROR', 'FATAL'];
        const normalized = level.toUpperCase();
        if (!validLevels.includes(normalized)) {
            throw new Error(`Invalid log level: ${level}. Must be one of: ${validLevels.join(', ')}`);
        }

        const payload = { key: 'log.level', value: normalized };

        if (edgeServerId === null || edgeServerId === 'all') {
            // 모든 클라이언트에 전파
            const results = await Promise.allSettled(
                Array.from(this.clients.values()).map(client =>
                    client.proxyRequest('POST', '/api/config', payload)
                )
            );
            const failed = results.filter(r => r.status === 'rejected');
            if (failed.length > 0) {
                console.warn(`⚠️ setLogLevel: ${failed.length}개 Collector 실패`);
            }
            return { success: true, level: normalized, propagated: results.length, failed: failed.length };
        }

        const client = await this.getClient(edgeServerId);
        await client.proxyRequest('POST', '/api/config', payload);
        return { success: true, level: normalized };
    }

    async gracefulShutdown() {
        for (const client of this.clients.values()) {
            client.shutdown();
        }
        console.log('✅ CollectorProxyService shutdown complete');
    }
}

// 싱글톤 인스턴스
let instance = null;
const RepositoryFactory = require('../database/repositories/RepositoryFactory');

module.exports = {
    getInstance: () => {
        if (!instance) {
            instance = new CollectorProxyService();
            process.on('SIGTERM', () => instance.gracefulShutdown());
            process.on('SIGINT', () => instance.gracefulShutdown());
        }
        return instance;
    },
    CollectorProxyService
};