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
    }
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

class CollectorProxyService {
  constructor() {
    this.config = ConfigManager.getInstance();
    this.setupConfiguration();
    this.setupConnectionPool();
    this.setupCircuitBreaker();
    this.setupHttpClient();
    this.setupHealthMonitoring();
    
    // 상태 추적
    this.lastHealthCheck = null;
    this.isHealthy = false;
    this.connectionAttempts = 0;
    this.lastConnectionTime = null;
    
    console.log(`🔧 Enhanced CollectorProxyService initialized: ${this.collectorConfig.host}:${this.collectorConfig.port}`);
  }

  setupConfiguration() {
    this.collectorConfig = {
      host: this.config.get('COLLECTOR_HOST', 'localhost'),
      port: this.config.getNumber('COLLECTOR_API_PORT', 8080),
      timeout: this.config.getNumber('COLLECTOR_TIMEOUT_MS', 5000),
      retries: this.config.getNumber('COLLECTOR_MAX_RETRIES', 3),
      enabled: this.config.getBoolean('COLLECTOR_ENABLED', true),
      
      // 새로운 고급 설정들
      connectionPoolSize: this.config.getNumber('COLLECTOR_POOL_SIZE', 20),
      keepAlive: this.config.getBoolean('COLLECTOR_KEEP_ALIVE', true),
      healthCheckInterval: this.config.getNumber('COLLECTOR_HEALTH_INTERVAL', 30000),
      circuitBreakerThreshold: this.config.getNumber('COLLECTOR_CB_THRESHOLD', 5),
      circuitBreakerTimeout: this.config.getNumber('COLLECTOR_CB_TIMEOUT', 30000)
    };
  }

  setupConnectionPool() {
    this.connectionPool = new ConnectionPool({
      maxSockets: this.collectorConfig.connectionPoolSize,
      keepAlive: this.collectorConfig.keepAlive,
      keepAliveMsecs: 1000,
      maxFreeSockets: Math.max(2, Math.floor(this.collectorConfig.connectionPoolSize / 5))
    });
  }

  setupCircuitBreaker() {
    this.circuitBreaker = new CircuitBreaker({
      failureThreshold: this.collectorConfig.circuitBreakerThreshold,
      recoveryTimeout: this.collectorConfig.circuitBreakerTimeout,
      monitoringPeriod: 10000
    });
  }

  setupHttpClient() {
    this.httpClient = axios.create({
        baseURL: `http://${this.collectorConfig.host}:${this.collectorConfig.port}`,
        timeout: this.collectorConfig.timeout,
        headers: {
        'Content-Type': 'application/json',
        'User-Agent': 'PulseOne-Backend/2.1.0-Enhanced',
        'Connection': 'keep-alive'
        },
        // HTTP Agent 사용 (https 대신)
        httpAgent: this.connectionPool.getAgent(),
        
        // 재시도 설정
        retry: this.collectorConfig.retries,
        retryDelay: (retryCount) => {
        return Math.min(1000 * Math.pow(2, retryCount), 10000);
        },
        
        validateStatus: (status) => {
        return status >= 200 && status < 300;
        }
    });

    this.setupInterceptors();
    }

  setupInterceptors() {
    // 요청 인터셉터
    this.httpClient.interceptors.request.use(
      (config) => {
        config.metadata = { startTime: Date.now() };
        console.log(`📤 Collector API Request: ${config.method.toUpperCase()} ${config.url}`);
        return config;
      },
      (error) => {
        console.error('📤 Request interceptor error:', error.message);
        return Promise.reject(error);
      }
    );

    // 응답 인터셉터 (메트릭스 포함)
    this.httpClient.interceptors.response.use(
      (response) => {
        const duration = Date.now() - response.config.metadata.startTime;
        console.log(`📥 Collector API Response: ${response.status} ${response.config.url} (${duration}ms)`);
        
        this.isHealthy = true;
        this.lastHealthCheck = new Date();
        this.lastConnectionTime = new Date();
        
        // 성공 메트릭스 기록
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
    // 주기적 Health Check
    this.healthCheckInterval = setInterval(async () => {
      try {
        await this.healthCheck();
      } catch (error) {
        console.warn('⚠️ Scheduled health check failed:', error.message);
      }
    }, this.collectorConfig.healthCheckInterval);

    // Connection Pool 모니터링
    this.poolMonitorInterval = setInterval(() => {
      const stats = this.connectionPool.getStats();
      console.log(`🔗 Connection Pool Stats: ${stats.totalSockets} total, ${stats.freeSockets} free`);
    }, 60000); // 1분마다
  }

  recordMetrics(type, statusCode, duration, errorCode = null) {
    // 메트릭스 기록 (향후 Prometheus/Grafana 연동 가능)
    const metrics = {
      timestamp: new Date(),
      type,
      statusCode,
      duration,
      errorCode,
      circuitBreakerState: this.circuitBreaker.getState().state
    };

    // 간단한 메모리 기반 메트릭스 (최대 1000개)
    if (!this.metrics) this.metrics = [];
    this.metrics.push(metrics);
    if (this.metrics.length > 1000) {
      this.metrics = this.metrics.slice(-1000);
    }
  }

  // =============================================================================
  // 🚀 강화된 API 메소드들 (Circuit Breaker 적용)
  // =============================================================================

  async healthCheck() {
    return await this.circuitBreaker.execute(
      async () => {
        const response = await this.httpClient.get('/api/health');
        this.isHealthy = true;
        this.lastHealthCheck = new Date();
        this.connectionAttempts = 0;
        
        return {
          success: true,
          data: response.data,
          timestamp: this.lastHealthCheck
        };
      },
      () => {
        // Circuit Breaker Fallback
        return {
          success: false,
          error: 'Circuit breaker is open',
          data: { status: 'circuit_open' },
          timestamp: new Date()
        };
      }
    );
  }

  async startDevice(deviceId, options = {}) {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() => 
        this.httpClient.post(`/api/devices/${deviceId}/worker/start`, {
          forceRestart: options.forceRestart || false,
          timeout: options.timeout || 30000
        })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  async stopDevice(deviceId, options = {}) {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() =>
        this.httpClient.post(`/api/devices/${deviceId}/worker/stop`, {
          graceful: options.graceful !== false,
          timeout: options.timeout || 10000
        })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  async restartDevice(deviceId, options = {}) {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() =>
        this.httpClient.post(`/api/devices/${deviceId}/worker/restart`, {
          timeout: options.timeout || 30000
        })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  async getDeviceStatus(deviceId) {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() =>
        this.httpClient.get(`/api/devices/${deviceId}/status`)
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  async getCurrentData(deviceId, pointIds = []) {
    return await this.circuitBreaker.execute(async () => {
      const params = pointIds.length > 0 ? { point_ids: pointIds.join(',') } : {};
      const response = await this.safeRequest(() =>
        this.httpClient.get(`/api/devices/${deviceId}/data/current`, { params })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  async getWorkerStatus() {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() =>
        this.httpClient.get('/api/workers/status')
      );
      
      return {
        success: true,
        data: response.data
      };
    });
  }

  async getSystemStatistics() {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() =>
        this.httpClient.get('/api/statistics')
      );
      
      return {
        success: true,
        data: response.data
      };
    });
  }

  // =============================================================================
  // 🔥 디바이스 제어 API (기존 호환성 유지)
  // =============================================================================

  async startDevice(deviceId, options = {}) {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() => 
        this.httpClient.post(`/api/devices/${deviceId}/worker/start`, {
          forceRestart: options.forceRestart || false,
          timeout: options.timeout || 30000
        })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  async stopDevice(deviceId, options = {}) {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() =>
        this.httpClient.post(`/api/devices/${deviceId}/worker/stop`, {
          graceful: options.graceful !== false,
          timeout: options.timeout || 10000
        })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  async restartDevice(deviceId, options = {}) {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() =>
        this.httpClient.post(`/api/devices/${deviceId}/worker/restart`, {
          timeout: options.timeout || 30000
        })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  async getDeviceStatus(deviceId) {
    return await this.circuitBreaker.execute(async () => {
      const response = await this.safeRequest(() =>
        this.httpClient.get(`/api/devices/${deviceId}/status`)
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  async getCurrentData(deviceId, pointIds = []) {
    return await this.circuitBreaker.execute(async () => {
      const params = pointIds.length > 0 ? { point_ids: pointIds.join(',') } : {};
      const response = await this.safeRequest(() =>
        this.httpClient.get(`/api/devices/${deviceId}/data/current`, { params })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    });
  }

  // =============================================================================
  // 🔥 설정 동기화 API (기존 호환성 유지)
  // =============================================================================

  async syncDeviceSettings(deviceId, settings) {
    try {
      const response = await this.safeRequest(() =>
        this.httpClient.post(`/api/devices/${deviceId}/sync`, settings)
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    } catch (error) {
      throw this.createProxyError('sync_device_settings', error, { deviceId });
    }
  }

  async reloadDeviceConfig(deviceId) {
    try {
      const response = await this.safeRequest(() =>
        this.httpClient.post(`/api/devices/${deviceId}/config/reload`)
      );
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    } catch (error) {
      throw this.createProxyError('reload_config', error, { deviceId });
    }
  }

  async reloadAllConfigs() {
    try {
      const response = await this.safeRequest(() =>
        this.httpClient.post('/api/config/reload')
      );
      
      return {
        success: true,
        data: response.data
      };
    } catch (error) {
      throw this.createProxyError('reload_all_configs', error);
    }
  }

  async notifyConfigChange(changeType, entityId, changes = {}) {
    try {
      const response = await this.safeRequest(() =>
        this.httpClient.post('/api/config/notify-change', {
          type: changeType, // 'device', 'alarm_rule', 'virtual_point'
          entity_id: entityId,
          changes,
          timestamp: new Date().toISOString()
        })
      );
      
      return {
        success: true,
        data: response.data
      };
    } catch (error) {
      throw this.createProxyError('notify_config_change', error, { changeType, entityId });
    }
  }

  // =============================================================================
  // 🔥 하드웨어 제어 API (기존 호환성 유지)
  // =============================================================================

  async controlDigitalOutput(deviceId, outputId, state, options = {}) {
    try {
      const response = await this.safeRequest(() =>
        this.httpClient.post(`/api/devices/${deviceId}/digital/${outputId}/control`, {
          state: Boolean(state),
          duration: options.duration,
          force: options.force || false
        })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId,
        outputId,
        state: Boolean(state)
      };
    } catch (error) {
      throw this.createProxyError('control_digital_output', error, { deviceId, outputId, state });
    }
  }

  async controlAnalogOutput(deviceId, outputId, value, options = {}) {
    try {
      const response = await this.safeRequest(() =>
        this.httpClient.post(`/api/devices/${deviceId}/analog/${outputId}/control`, {
          value: Number(value),
          unit: options.unit,
          ramp_time: options.rampTime
        })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId,
        outputId,
        value: Number(value)
      };
    } catch (error) {
      throw this.createProxyError('control_analog_output', error, { deviceId, outputId, value });
    }
  }

  async controlPump(deviceId, pumpId, enable, options = {}) {
    try {
      const response = await this.safeRequest(() =>
        this.httpClient.post(`/api/devices/${deviceId}/pump/${pumpId}/control`, {
          enable: Boolean(enable),
          speed: options.speed || 100,
          duration: options.duration
        })
      );
      
      return {
        success: true,
        data: response.data,
        deviceId,
        pumpId,
        enable: Boolean(enable)
      };
    } catch (error) {
      throw this.createProxyError('control_pump', error, { deviceId, pumpId, enable });
    }
  }

  // =============================================================================
  // 🔧 배치 처리 및 성능 최적화
  // =============================================================================

  async batchDeviceControl(operations) {
    if (!Array.isArray(operations) || operations.length === 0) {
      throw new Error('Operations array is required');
    }

    console.log(`🚀 Batch device control: ${operations.length} operations`);
    
    // 동시 실행 제한 (최대 10개씩)
    const BATCH_SIZE = 10;
    const results = [];
    
    for (let i = 0; i < operations.length; i += BATCH_SIZE) {
      const batch = operations.slice(i, i + BATCH_SIZE);
      
      const batchPromises = batch.map(async (op) => {
        try {
          let result;
          
          switch (op.action) {
            case 'start':
              result = await this.startDevice(op.deviceId, op.options);
              break;
            case 'stop':
              result = await this.stopDevice(op.deviceId, op.options);
              break;
            case 'restart':
              result = await this.restartDevice(op.deviceId, op.options);
              break;
            case 'status':
              result = await this.getDeviceStatus(op.deviceId);
              break;
            default:
              throw new Error(`Unknown action: ${op.action}`);
          }
          
          return {
            deviceId: op.deviceId,
            action: op.action,
            success: true,
            data: result.data
          };
          
        } catch (error) {
          return {
            deviceId: op.deviceId,
            action: op.action,
            success: false,
            error: error.message
          };
        }
      });
      
      const batchResults = await Promise.allSettled(batchPromises);
      results.push(...batchResults.map(r => r.value || { success: false, error: 'Promise rejected' }));
      
      // 배치 간 짧은 대기 (Collector 부하 방지)
      if (i + BATCH_SIZE < operations.length) {
        await new Promise(resolve => setTimeout(resolve, 100));
      }
    }
    
    const successful = results.filter(r => r.success).length;
    const failed = results.filter(r => !r.success).length;
    
    console.log(`✅ Batch completed: ${successful} successful, ${failed} failed`);
    
    return {
      success: failed === 0,
      results,
      summary: { total: results.length, successful, failed }
    };
  }

  // =============================================================================
  // 유틸리티 및 모니터링 메소드
  // =============================================================================

  handleApiError(error) {
    this.isHealthy = false;
    this.connectionAttempts++;
    
    if (error.code === 'ECONNREFUSED') {
      console.warn(`🔌 Collector 연결 실패 (${this.connectionAttempts}회): ${this.collectorConfig.host}:${this.collectorConfig.port}`);
    } else if (error.code === 'ECONNABORTED') {
      console.warn(`⏱️ Collector 타임아웃: ${this.collectorConfig.timeout}ms`);
    } else if (error.response) {
      console.warn(`❌ Collector API 에러: ${error.response.status} ${error.response.statusText}`);
    } else {
      console.warn(`🚨 Collector 통신 에러: ${error.message}`);
    }
  }

  async safeRequest(requestFn, maxRetries = null) {
    const retries = maxRetries || this.collectorConfig.retries;
    let lastError;
    
    for (let attempt = 1; attempt <= retries; attempt++) {
      try {
        return await requestFn();
      } catch (error) {
        lastError = error;
        
        if (attempt < retries) {
          const delay = Math.min(1000 * Math.pow(2, attempt - 1), 5000);
          console.log(`🔄 Collector API 재시도 ${attempt}/${retries} (${delay}ms 후)`);
          await new Promise(resolve => setTimeout(resolve, delay));
        }
      }
    }
    
    throw lastError;
  }

  getConnectionStats() {
    return {
      isHealthy: this.isHealthy,
      lastHealthCheck: this.lastHealthCheck,
      lastConnectionTime: this.lastConnectionTime,
      connectionAttempts: this.connectionAttempts,
      circuitBreakerState: this.circuitBreaker.getState(),
      connectionPoolStats: this.connectionPool.getStats(),
      recentMetrics: this.metrics?.slice(-10) || []
    };
  }

  async gracefulShutdown() {
    console.log('🔄 CollectorProxyService graceful shutdown...');
    
    // Health check 중지
    if (this.healthCheckInterval) {
      clearInterval(this.healthCheckInterval);
    }
    
    if (this.poolMonitorInterval) {
      clearInterval(this.poolMonitorInterval);
    }
    
    // Connection pool 정리
    this.connectionPool.destroy();
    
    console.log('✅ CollectorProxyService shutdown completed');
  }

  // Getter methods (기존 호환성 유지)
  isCollectorHealthy() { return this.isHealthy; }
  getLastHealthCheck() { return this.lastHealthCheck; }
  getCollectorConfig() { return { ...this.collectorConfig }; }
  
  createProxyError(operation, originalError, context = {}) {
    const error = new Error(`Collector API Error: ${operation}`);
    error.name = 'CollectorProxyError';
    error.operation = operation;
    error.context = context;
    error.collectorError = originalError.message;
    
    if (originalError.response) {
      error.status = originalError.response.status;
      error.statusText = originalError.response.statusText;
      error.collectorResponse = originalError.response.data;
    } else if (originalError.code) {
      error.code = originalError.code;
    }
    
    return error;
  }
}

// 싱글톤 인스턴스
let instance = null;

module.exports = {
  getInstance: () => {
    if (!instance) {
      instance = new CollectorProxyService();
      
      // Graceful shutdown 핸들러
      process.on('SIGTERM', () => instance.gracefulShutdown());
      process.on('SIGINT', () => instance.gracefulShutdown());
    }
    return instance;
  },
  CollectorProxyService
};