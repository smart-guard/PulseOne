// =============================================================================
// backend/lib/services/CollectorProxyService.js
// Backend → Collector HTTP API 프록시 서비스 (ConfigManager 통합)
// =============================================================================

const axios = require('axios');
const ConfigManager = require('../config/ConfigManager');

class CollectorProxyService {
  constructor() {
    this.config = ConfigManager.getInstance();
    
    // ConfigManager에서 Collector 설정 읽기
    this.collectorConfig = {
      host: this.config.get('COLLECTOR_HOST', 'localhost'),
      port: this.config.getNumber('COLLECTOR_API_PORT', 8080),
      timeout: this.config.getNumber('COLLECTOR_TIMEOUT_MS', 5000),
      retries: this.config.getNumber('COLLECTOR_MAX_RETRIES', 3),
      enabled: this.config.getBoolean('COLLECTOR_ENABLED', true)
    };
    
    // HTTP 클라이언트 초기화
    this.httpClient = axios.create({
      baseURL: `http://${this.collectorConfig.host}:${this.collectorConfig.port}`,
      timeout: this.collectorConfig.timeout,
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': 'PulseOne-Backend/2.1.0'
      }
    });
    
    // 응답/에러 인터셉터 설정
    this.setupInterceptors();
    
    // 상태 추적
    this.lastHealthCheck = null;
    this.isHealthy = false;
    
    console.log(`🔧 CollectorProxyService initialized: ${this.collectorConfig.host}:${this.collectorConfig.port}`);
  }

  setupInterceptors() {
    // 요청 인터셉터 (로깅)
    this.httpClient.interceptors.request.use(
      (config) => {
        console.log(`📤 Collector API Request: ${config.method.toUpperCase()} ${config.url}`);
        return config;
      },
      (error) => Promise.reject(error)
    );

    // 응답 인터셉터 (에러 처리)
    this.httpClient.interceptors.response.use(
      (response) => {
        console.log(`📥 Collector API Response: ${response.status} ${response.config.url}`);
        this.isHealthy = true;
        this.lastHealthCheck = new Date();
        return response;
      },
      (error) => {
        this.handleApiError(error);
        return Promise.reject(error);
      }
    );
  }

  handleApiError(error) {
    this.isHealthy = false;
    
    if (error.code === 'ECONNREFUSED') {
      console.warn(`🔌 Collector 연결 실패: ${this.collectorConfig.host}:${this.collectorConfig.port}`);
    } else if (error.code === 'ECONNABORTED') {
      console.warn(`⏱️ Collector 타임아웃: ${this.collectorConfig.timeout}ms`);
    } else if (error.response) {
      console.warn(`❌ Collector API 에러: ${error.response.status} ${error.response.statusText}`);
    } else {
      console.warn(`🚨 Collector 통신 에러: ${error.message}`);
    }
  }

  // =============================================================================
  // 🔥 1. 디바이스 제어 API (최우선)
  // =============================================================================

  async startDevice(deviceId, options = {}) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/worker/start`, {
        force_restart: options.forceRestart || false,
        ...options
      });
      
      return {
        success: true,
        data: response.data,
        deviceId,
        action: 'start'
      };
    } catch (error) {
      throw this.createProxyError('start_device', error, { deviceId });
    }
  }

  async stopDevice(deviceId, options = {}) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/worker/stop`, {
        graceful: options.graceful !== false,
        ...options
      });
      
      return {
        success: true,
        data: response.data,
        deviceId,
        action: 'stop'
      };
    } catch (error) {
      throw this.createProxyError('stop_device', error, { deviceId });
    }
  }

  async pauseDevice(deviceId) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/worker/pause`);
      
      return {
        success: true,
        data: response.data,
        deviceId,
        action: 'pause'
      };
    } catch (error) {
      throw this.createProxyError('pause_device', error, { deviceId });
    }
  }

  async resumeDevice(deviceId) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/worker/resume`);
      
      return {
        success: true,
        data: response.data,
        deviceId,
        action: 'resume'
      };
    } catch (error) {
      throw this.createProxyError('resume_device', error, { deviceId });
    }
  }

  async restartDevice(deviceId, options = {}) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/worker/restart`, {
        wait_for_completion: options.wait !== false,
        ...options
      });
      
      return {
        success: true,
        data: response.data,
        deviceId,
        action: 'restart'
      };
    } catch (error) {
      throw this.createProxyError('restart_device', error, { deviceId });
    }
  }

  async getDeviceStatus(deviceId) {
    try {
      const response = await this.httpClient.get(`/api/devices/${deviceId}/status`);
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    } catch (error) {
      throw this.createProxyError('get_device_status', error, { deviceId });
    }
  }

  // =============================================================================
  // 🔥 2. 하드웨어 제어 API
  // =============================================================================

  async controlDigitalOutput(deviceId, outputId, state, options = {}) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/digital/${outputId}/control`, {
        state: Boolean(state),
        duration_ms: options.duration,
        force: options.force || false
      });
      
      return {
        success: true,
        data: response.data,
        deviceId,
        outputId,
        action: 'digital_control'
      };
    } catch (error) {
      throw this.createProxyError('digital_control', error, { deviceId, outputId });
    }
  }

  async controlAnalogOutput(deviceId, outputId, value, options = {}) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/analog/${outputId}/control`, {
        value: Number(value),
        unit: options.unit,
        ramp_time_ms: options.rampTime
      });
      
      return {
        success: true,
        data: response.data,
        deviceId,
        outputId,
        action: 'analog_control'
      };
    } catch (error) {
      throw this.createProxyError('analog_control', error, { deviceId, outputId });
    }
  }

  async controlPump(deviceId, pumpId, enable, options = {}) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/pump/${pumpId}/control`, {
        enable: Boolean(enable),
        speed_percent: options.speed || 100,
        duration_ms: options.duration
      });
      
      return {
        success: true,
        data: response.data,
        deviceId,
        pumpId,
        action: 'pump_control'
      };
    } catch (error) {
      throw this.createProxyError('pump_control', error, { deviceId, pumpId });
    }
  }

  async setParameter(deviceId, parameterId, value, options = {}) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/parameters/${parameterId}/set`, {
        value,
        data_type: options.dataType || 'auto',
        validate: options.validate !== false
      });
      
      return {
        success: true,
        data: response.data,
        deviceId,
        parameterId,
        action: 'parameter_change'
      };
    } catch (error) {
      throw this.createProxyError('parameter_change', error, { deviceId, parameterId });
    }
  }

  // =============================================================================
  // 🔥 3. 실시간 데이터 API
  // =============================================================================

  async getCurrentData(deviceId, pointIds = []) {
    try {
      const params = pointIds.length > 0 ? { point_ids: pointIds.join(',') } : {};
      const response = await this.httpClient.get(`/api/devices/${deviceId}/data/current`, { params });
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    } catch (error) {
      throw this.createProxyError('get_current_data', error, { deviceId });
    }
  }

  async getActiveAlarms(options = {}) {
    try {
      const params = {
        severity: options.severity,
        device_id: options.deviceId,
        limit: options.limit || 50,
        acknowledged: options.acknowledged
      };
      
      const response = await this.httpClient.get('/api/alarms/active', { params });
      
      return {
        success: true,
        data: response.data
      };
    } catch (error) {
      throw this.createProxyError('get_active_alarms', error, options);
    }
  }

  async acknowledgeAlarm(alarmId, userId, comment = '') {
    try {
      const response = await this.httpClient.post(`/api/alarms/${alarmId}/acknowledge`, {
        user_id: userId,
        comment,
        timestamp: new Date().toISOString()
      });
      
      return {
        success: true,
        data: response.data,
        alarmId
      };
    } catch (error) {
      throw this.createProxyError('acknowledge_alarm', error, { alarmId });
    }
  }

  async clearAlarm(alarmId, userId, comment = '') {
    try {
      const response = await this.httpClient.post(`/api/alarms/${alarmId}/clear`, {
        user_id: userId,
        comment,
        timestamp: new Date().toISOString()
      });
      
      return {
        success: true,
        data: response.data,
        alarmId
      };
    } catch (error) {
      throw this.createProxyError('clear_alarm', error, { alarmId });
    }
  }

  // =============================================================================
  // 🔥 4. 설정 동기화 API
  // =============================================================================

  async reloadDeviceConfig(deviceId) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/config/reload`);
      
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
      const response = await this.httpClient.post('/api/config/reload');
      
      return {
        success: true,
        data: response.data
      };
    } catch (error) {
      throw this.createProxyError('reload_all_configs', error);
    }
  }

  async syncDeviceSettings(deviceId, settings) {
    try {
      const response = await this.httpClient.post(`/api/devices/${deviceId}/sync`, settings);
      
      return {
        success: true,
        data: response.data,
        deviceId
      };
    } catch (error) {
      throw this.createProxyError('sync_device_settings', error, { deviceId });
    }
  }

  async notifyConfigChange(changeType, entityId, changes = {}) {
    try {
      const response = await this.httpClient.post('/api/config/notify-change', {
        type: changeType, // 'device', 'alarm_rule', 'virtual_point'
        entity_id: entityId,
        changes,
        timestamp: new Date().toISOString()
      });
      
      return {
        success: true,
        data: response.data
      };
    } catch (error) {
      throw this.createProxyError('notify_config_change', error, { changeType, entityId });
    }
  }

  // =============================================================================
  // 🔥 5. 시스템 관리 API
  // =============================================================================

  async getSystemStatistics() {
    try {
      const response = await this.httpClient.get('/api/statistics');
      
      return {
        success: true,
        data: response.data
      };
    } catch (error) {
      throw this.createProxyError('get_statistics', error);
    }
  }

  async getWorkerStatus() {
    try {
      const response = await this.httpClient.get('/api/workers/status');
      
      return {
        success: true,
        data: response.data
      };
    } catch (error) {
      throw this.createProxyError('get_worker_status', error);
    }
  }

  async healthCheck() {
    try {
      const response = await this.httpClient.get('/api/health');
      
      this.isHealthy = true;
      this.lastHealthCheck = new Date();
      
      return {
        success: true,
        data: response.data,
        timestamp: this.lastHealthCheck
      };
    } catch (error) {
      this.isHealthy = false;
      throw this.createProxyError('health_check', error);
    }
  }

  // =============================================================================
  // 유틸리티 메서드
  // =============================================================================

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

  isCollectorHealthy() {
    return this.isHealthy;
  }

  getLastHealthCheck() {
    return this.lastHealthCheck;
  }

  getCollectorConfig() {
    return { ...this.collectorConfig };
  }

  // 재시도 로직이 포함된 안전한 요청
  async safeRequest(requestFn, maxRetries = null) {
    const retries = maxRetries || this.collectorConfig.retries;
    let lastError;
    
    for (let attempt = 1; attempt <= retries; attempt++) {
      try {
        return await requestFn();
      } catch (error) {
        lastError = error;
        
        if (attempt < retries) {
          const delay = Math.min(1000 * Math.pow(2, attempt - 1), 5000); // 지수 백오프
          console.log(`🔄 Collector API 재시도 ${attempt}/${retries} (${delay}ms 후)`);
          await new Promise(resolve => setTimeout(resolve, delay));
        }
      }
    }
    
    throw lastError;
  }
}

// 싱글톤 인스턴스
let instance = null;

module.exports = {
  getInstance: () => {
    if (!instance) {
      instance = new CollectorProxyService();
    }
    return instance;
  },
  CollectorProxyService
};