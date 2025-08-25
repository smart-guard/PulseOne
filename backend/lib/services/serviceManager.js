// backend/lib/services/serviceManager.js
// 분산 서비스 관리 및 제어 - HTTP API 통신 기능 추가

const { spawn, exec } = require('child_process');
const axios = require('axios');
const path = require('path');
const fs = require('fs').promises;
const ConfigManager = require('../config/ConfigManager');

class ServiceManager {
  constructor() {
    this.services = new Map();
    this.remoteNodes = new Map();
    this.serviceConfigs = new Map();
    
    // ConfigManager 연동
    const config = ConfigManager.getInstance();
    this.config = {
      collector: {
        host: config.get('COLLECTOR_HOST', 'localhost'),
        port: config.getNumber('COLLECTOR_API_PORT', 8080),
        timeout: config.getNumber('COLLECTOR_TIMEOUT_MS', 5000)
      }
    };

    // HTTP 클라이언트 설정
    this.httpClient = axios.create({
      timeout: this.config.collector.timeout,
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': 'PulseOne-ServiceManager/1.0'
      }
    });
    
    this.setupHttpInterceptors();
  }

  setupHttpInterceptors() {
    this.httpClient.interceptors.response.use(
      (response) => response,
      (error) => {
        if (error.code === 'ECONNREFUSED') {
          // 연결 거부는 로그 레벨 낮춤 (정상적인 상황)
          return Promise.reject(error);
        }
        console.warn(`HTTP 통신 오류: ${error.message}`);
        return Promise.reject(error);
      }
    );
  }

  // ========================================
  // 로컬 서비스 관리 (프로세스 제어)
  // ========================================

  async startCollector(config = {}) {
    console.log('Collector 서비스 시작 중...');
    
    try {
      // 이미 실행 중인지 확인
      const existingService = this.services.get('collector');
      if (existingService && existingService.status === 'running') {
        console.log('Collector가 이미 실행 중입니다');
        return { success: true, pid: existingService.pid, alreadyRunning: true };
      }

      // Collector 설정 파일 생성
      await this.generateCollectorConfig(config);
      
      // 플랫폼별 실행 파일 경로
      const isWindows = process.platform === 'win32';
      const collectorExecutable = isWindows ? 'collector.exe' : 'collector';
      const collectorPath = path.join(__dirname, '../../../collector/bin', collectorExecutable);
      
      // 실행 파일 존재 확인
      const fs = require('fs');
      if (!fs.existsSync(collectorPath)) {
        throw new Error(`Collector 실행 파일을 찾을 수 없습니다: ${collectorPath}`);
      }
      
      // Collector 프로세스 시작
      const spawnOptions = {
        cwd: path.join(__dirname, '../../../collector'),
        stdio: ['pipe', 'pipe', 'pipe'],
        env: { ...process.env, ...config.env }
      };

      // Windows에서 shell 옵션 추가
      if (isWindows) {
        spawnOptions.shell = true;
      }

      const collectorProcess = spawn(collectorPath, [], spawnOptions);

      // 프로세스 이벤트 처리
      collectorProcess.stdout.on('data', (data) => {
        console.log(`[Collector] ${data.toString().trim()}`);
      });

      collectorProcess.stderr.on('data', (data) => {
        console.error(`[Collector Error] ${data.toString().trim()}`);
      });

      collectorProcess.on('close', (code) => {
        console.log(`Collector 프로세스 종료됨 (코드: ${code})`);
        this.services.delete('collector');
      });

      collectorProcess.on('error', (error) => {
        console.error('Collector 프로세스 오류:', error.message);
        this.services.delete('collector');
      });

      // 서비스 등록
      this.services.set('collector', {
        process: collectorProcess,
        pid: collectorProcess.pid,
        status: 'starting',
        startTime: new Date(),
        config: config,
        httpReady: false
      });

      console.log(`Collector 프로세스 시작됨 (PID: ${collectorProcess.pid})`);

      // HTTP API 준비 대기
      const httpReady = await this.waitForHttpApi('collector', 30000);
      if (httpReady) {
        const service = this.services.get('collector');
        service.status = 'running';
        service.httpReady = true;
        console.log('Collector HTTP API 준비 완료');
      } else {
        console.warn('Collector HTTP API 준비 실패, 프로세스만 실행 중');
        const service = this.services.get('collector');
        service.status = 'running_no_api';
      }

      return { 
        success: true, 
        pid: collectorProcess.pid, 
        httpReady: httpReady 
      };

    } catch (error) {
      console.error('Collector 시작 실패:', error.message);
      return { success: false, error: error.message };
    }
  }

  async stopCollector() {
    console.log('Collector 서비스 중지 중...');
    
    const service = this.services.get('collector');
    if (!service) {
      return { success: false, error: 'Collector가 실행 중이지 않습니다' };
    }

    try {
      // HTTP API로 graceful shutdown 시도
      if (service.httpReady) {
        try {
          await this.sendCollectorCommand('stop');
          console.log('Collector에 중지 명령 전송됨');
        } catch (error) {
          console.warn('HTTP 중지 명령 실패, 프로세스 직접 종료:', error.message);
        }
      }

      // 플랫폼별 프로세스 종료
      const isWindows = process.platform === 'win32';
      
      if (isWindows) {
        // Windows: taskkill 사용
        const { exec } = require('child_process');
        
        // Graceful shutdown 시도
        exec(`taskkill /pid ${service.pid} /f`, (error) => {
          if (error) {
            console.warn('Windows 프로세스 종료 실패:', error.message);
          }
        });
      } else {
        // Linux/macOS: SIGTERM 사용
        service.process.kill('SIGTERM');
        
        // Force kill after 10 seconds
        const forceKillTimeout = setTimeout(() => {
          if (this.services.has('collector')) {
            console.warn('강제 종료 실행 (SIGKILL)');
            service.process.kill('SIGKILL');
          }
        }, 10000);

        // 프로세스 종료 대기
        return new Promise((resolve) => {
          service.process.on('close', () => {
            clearTimeout(forceKillTimeout);
            this.services.delete('collector');
            console.log('Collector 중지 완료');
            resolve({ success: true });
          });
        });
      }

      // Windows의 경우 동기적 처리
      if (isWindows) {
        setTimeout(() => {
          this.services.delete('collector');
        }, 2000);
        return { success: true };
      }

    } catch (error) {
      console.error('Collector 중지 실패:', error.message);
      return { success: false, error: error.message };
    }
  }

  async restartCollector(config = {}) {
    console.log('Collector 서비스 재시작 중...');
    
    const stopResult = await this.stopCollector();
    if (!stopResult.success) {
      return stopResult;
    }
    
    // 잠시 대기
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    return await this.startCollector(config);
  }

  // ========================================
  // HTTP API 통신 (실행 중인 Collector와 통신)
  // ========================================

  async waitForHttpApi(serviceName, timeoutMs = 30000) {
    const service = this.services.get(serviceName);
    if (!service) {
      return false;
    }

    const startTime = Date.now();
    const checkInterval = 2000;

    while (Date.now() - startTime < timeoutMs) {
      try {
        const baseUrl = `http://${this.config.collector.host}:${this.config.collector.port}`;
        const response = await this.httpClient.get(`${baseUrl}/api/health`);
        
        if (response.status === 200) {
          return true;
        }
      } catch (error) {
        // 연결 실패는 정상 (아직 시작 중)
      }

      await new Promise(resolve => setTimeout(resolve, checkInterval));
    }

    return false;
  }

  async sendCollectorCommand(action, params = {}) {
    const service = this.services.get('collector');
    if (!service || !service.httpReady) {
      throw new Error('Collector HTTP API를 사용할 수 없습니다');
    }

    const baseUrl = `http://${this.config.collector.host}:${this.config.collector.port}`;
    
    switch (action) {
      case 'status':
        return await this.httpClient.get(`${baseUrl}/api/lifecycle/status`);
      case 'stop':
        return await this.httpClient.post(`${baseUrl}/api/lifecycle/stop`);
      case 'restart':
        return await this.httpClient.post(`${baseUrl}/api/lifecycle/restart`);
      case 'reload_config':
        return await this.httpClient.post(`${baseUrl}/api/config/reload`);
      case 'statistics':
        return await this.httpClient.get(`${baseUrl}/api/statistics`);
      case 'workers':
        return await this.httpClient.get(`${baseUrl}/api/workers/status`);
      case 'device_status':
        const deviceId = params.deviceId;
        const url = deviceId ? 
          `${baseUrl}/api/devices/${deviceId}/status` : 
          `${baseUrl}/api/devices/status`;
        return await this.httpClient.get(url);
      case 'device_control':
        return await this.httpClient.post(`${baseUrl}/api/devices/${params.deviceId}/control`, {
          action: params.controlAction,
          params: params.controlParams || {}
        });
      default:
        throw new Error(`지원하지 않는 명령: ${action}`);
    }
  }

  async getCollectorStatus() {
    try {
      const response = await this.sendCollectorCommand('status');
      return {
        success: true,
        isRunning: response.data?.isRunning || false,
        status: response.data?.status || 'unknown',
        ...response.data
      };
    } catch (error) {
      const processStatus = this.getServiceStatus('collector');
      return {
        success: false,
        error: error.message,
        processStatus: processStatus
      };
    }
  }

  async getCollectorStatistics() {
    try {
      const response = await this.sendCollectorCommand('statistics');
      return { success: true, data: response.data };
    } catch (error) {
      return { success: false, error: error.message };
    }
  }

  async controlDevice(deviceId, action, params = {}) {
    try {
      const response = await this.sendCollectorCommand('device_control', {
        deviceId,
        controlAction: action,
        controlParams: params
      });
      return { success: true, data: response.data };
    } catch (error) {
      return { success: false, error: error.message };
    }
  }

  // ========================================
  // 원격 서비스 관리 (기존 코드 유지)
  // ========================================

  async registerRemoteNode(nodeInfo) {
    const { nodeId, host, port, services, location } = nodeInfo;
    
    this.remoteNodes.set(nodeId, {
      host,
      port,
      services,
      location,
      status: 'connected',
      lastSeen: new Date(),
      ...nodeInfo
    });

    console.log(`원격 노드 등록됨: ${nodeId} (${host}:${port})`);
    return { success: true };
  }

  async controlRemoteService(nodeId, serviceName, action, config = {}) {
    const node = this.remoteNodes.get(nodeId);
    if (!node) {
      return { success: false, error: 'Node not found' };
    }

    try {
      const response = await this.sendRemoteCommand(node, {
        action,
        service: serviceName,
        config
      });

      console.log(`원격 명령 전송됨: ${nodeId} - ${action} ${serviceName}`);
      return response;

    } catch (error) {
      console.error(`원격 명령 실패 ${nodeId}:`, error.message);
      return { success: false, error: error.message };
    }
  }

  async sendRemoteCommand(node, command) {
    return new Promise((resolve, reject) => {
      const http = require('http');
      
      const postData = JSON.stringify(command);
      const options = {
        hostname: node.host,
        port: node.port,
        path: '/api/service/control',
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Content-Length': Buffer.byteLength(postData)
        }
      };

      const req = http.request(options, (res) => {
        let data = '';
        res.on('data', (chunk) => data += chunk);
        res.on('end', () => {
          try {
            resolve(JSON.parse(data));
          } catch (error) {
            reject(error);
          }
        });
      });

      req.on('error', reject);
      req.write(postData);
      req.end();
    });
  }

  // ========================================
  // 서비스 상태 모니터링
  // ========================================

  getServiceStatus(serviceName) {
    const service = this.services.get(serviceName);
    if (!service) {
      return { status: 'stopped', message: 'Service not running' };
    }

    return {
      status: service.status,
      pid: service.pid,
      startTime: service.startTime,
      uptime: Date.now() - service.startTime.getTime(),
      config: service.config,
      httpReady: service.httpReady || false
    };
  }

  getAllServicesStatus() {
    const localServices = {};
    const remoteServices = {};

    // 로컬 서비스 상태
    for (const [name, service] of this.services) {
      localServices[name] = this.getServiceStatus(name);
    }

    // 원격 서비스 상태
    for (const [nodeId, node] of this.remoteNodes) {
      remoteServices[nodeId] = {
        host: node.host,
        port: node.port,
        location: node.location,
        status: node.status,
        lastSeen: node.lastSeen,
        services: node.services
      };
    }

    return {
      local: localServices,
      remote: remoteServices,
      timestamp: new Date()
    };
  }

  // ========================================
  // 설정 관리
  // ========================================

  async generateCollectorConfig(config) {
    const configManager = ConfigManager.getInstance();
    
    const defaultConfig = {
      database: {
        redis: {
          host: configManager.get('REDIS_PRIMARY_HOST', 'localhost'),
          port: configManager.getNumber('REDIS_PRIMARY_PORT', 6379),
          password: configManager.get('REDIS_PRIMARY_PASSWORD', '')
        },
        sqlite: {
          path: configManager.get('SQLITE_PATH', './data/pulseone.db')
        }
      },
      message_queue: {
        rabbitmq: {
          host: configManager.get('RABBITMQ_HOST', 'localhost'),
          port: configManager.getNumber('RABBITMQ_PORT', 5672),
          user: configManager.get('RABBITMQ_USER', 'guest'),
          password: configManager.get('RABBITMQ_PASSWORD', 'guest')
        }
      },
      collection: {
        polling_interval: config.polling_interval || 1000,
        batch_size: config.batch_size || 100,
        max_retries: config.max_retries || 3
      },
      api: {
        port: configManager.getNumber('COLLECTOR_API_PORT', 8080),
        host: configManager.get('COLLECTOR_API_HOST', '0.0.0.0')
      },
      drivers: config.drivers || []
    };

    const finalConfig = { ...defaultConfig, ...config };
    
    const configPath = path.join(__dirname, '../../../collector/config/runtime.json');
    const configDir = path.dirname(configPath);
    
    // 디렉토리 생성
    await fs.mkdir(configDir, { recursive: true });
    await fs.writeFile(configPath, JSON.stringify(finalConfig, null, 2));
    
    console.log(`Collector 설정 파일 생성됨: ${configPath}`);
    return finalConfig;
  }

  async saveServiceConfig(serviceName, config) {
    this.serviceConfigs.set(serviceName, config);
    
    const configDir = path.join(__dirname, '../config');
    await fs.mkdir(configDir, { recursive: true });
    
    const configPath = path.join(configDir, 'services.json');
    const allConfigs = Object.fromEntries(this.serviceConfigs);
    await fs.writeFile(configPath, JSON.stringify(allConfigs, null, 2));
    
    console.log(`서비스 설정 저장됨: ${serviceName}`);
  }

  async loadServiceConfigs() {
    try {
      const configPath = path.join(__dirname, '../config/services.json');
      const data = await fs.readFile(configPath, 'utf8');
      const configs = JSON.parse(data);
      
      for (const [serviceName, config] of Object.entries(configs)) {
        this.serviceConfigs.set(serviceName, config);
      }
      
      console.log('서비스 설정 로드됨');
    } catch (error) {
      console.log('기존 서비스 설정 없음, 기본값 사용');
    }
  }

  // ========================================
  // 헬스 체크
  // ========================================

  async performHealthCheck() {
    const health = {
      timestamp: new Date(),
      local_services: {},
      remote_nodes: {},
      overall_status: 'healthy'
    };

    // 로컬 서비스 헬스 체크
    for (const [serviceName, service] of this.services) {
      try {
        const processHealthy = await this.checkServiceHealth(service);
        let httpHealthy = false;
        
        if (service.httpReady) {
          try {
            const response = await this.sendCollectorCommand('status');
            httpHealthy = response.status === 200;
          } catch (error) {
            httpHealthy = false;
          }
        }

        health.local_services[serviceName] = {
          process_status: processHealthy ? 'healthy' : 'unhealthy',
          http_status: service.httpReady ? (httpHealthy ? 'healthy' : 'unhealthy') : 'disabled',
          pid: service.pid,
          uptime: Date.now() - service.startTime.getTime(),
          overall_status: (processHealthy && (httpHealthy || !service.httpReady)) ? 'healthy' : 'unhealthy'
        };
        
        if (health.local_services[serviceName].overall_status === 'unhealthy') {
          health.overall_status = 'degraded';
        }
      } catch (error) {
        health.local_services[serviceName] = {
          status: 'error',
          error: error.message
        };
        health.overall_status = 'degraded';
      }
    }

    // 원격 노드 헬스 체크 (기존 코드 유지)
    for (const [nodeId, node] of this.remoteNodes) {
      try {
        const response = await this.sendRemoteCommand(node, { action: 'health_check' });
        health.remote_nodes[nodeId] = {
          status: response.success ? 'healthy' : 'unhealthy',
          response_time: response.response_time,
          last_check: new Date()
        };
      } catch (error) {
        health.remote_nodes[nodeId] = {
          status: 'unreachable',
          error: error.message,
          last_check: new Date()
        };
        health.overall_status = 'degraded';
      }
    }

    return health;
  }

  async checkServiceHealth(service) {
    try {
      process.kill(service.pid, 0);
      return true;
    } catch (error) {
      return false;
    }
  }
}

module.exports = new ServiceManager();