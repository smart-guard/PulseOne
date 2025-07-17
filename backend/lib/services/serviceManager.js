// backend/lib/services/serviceManager.js
// 분산 서비스 관리 및 제어

const { spawn, exec } = require('child_process');
const path = require('path');
const fs = require('fs').promises;

class ServiceManager {
  constructor() {
    this.services = new Map();
    this.remoteNodes = new Map();
    this.serviceConfigs = new Map();
  }

  // ========================================
  // 로컬 서비스 관리
  // ========================================

  async startCollector(config = {}) {
    console.log('🚀 Starting Collector service...');
    
    try {
      // Collector 설정 파일 생성
      await this.generateCollectorConfig(config);
      
      // Collector 프로세스 시작
      const collectorPath = path.join(__dirname, '../../../collector/bin/collector');
      const collectorProcess = spawn(collectorPath, [], {
        cwd: path.join(__dirname, '../../../collector'),
        stdio: ['pipe', 'pipe', 'pipe'],
        env: { ...process.env, ...config.env }
      });

      // 프로세스 이벤트 처리
      collectorProcess.stdout.on('data', (data) => {
        console.log(`[Collector] ${data.toString()}`);
      });

      collectorProcess.stderr.on('data', (data) => {
        console.error(`[Collector Error] ${data.toString()}`);
      });

      collectorProcess.on('close', (code) => {
        console.log(`Collector process exited with code ${code}`);
        this.services.delete('collector');
      });

      // 서비스 등록
      this.services.set('collector', {
        process: collectorProcess,
        pid: collectorProcess.pid,
        status: 'running',
        startTime: new Date(),
        config: config
      });

      console.log(`✅ Collector started with PID: ${collectorProcess.pid}`);
      return { success: true, pid: collectorProcess.pid };

    } catch (error) {
      console.error('❌ Failed to start Collector:', error);
      return { success: false, error: error.message };
    }
  }

  async stopCollector() {
    console.log('🛑 Stopping Collector service...');
    
    const service = this.services.get('collector');
    if (!service) {
      return { success: false, error: 'Collector not running' };
    }

    try {
      // Graceful shutdown
      service.process.kill('SIGTERM');
      
      // Force kill after 10 seconds
      setTimeout(() => {
        if (this.services.has('collector')) {
          service.process.kill('SIGKILL');
        }
      }, 10000);

      this.services.delete('collector');
      console.log('✅ Collector stopped');
      return { success: true };

    } catch (error) {
      console.error('❌ Failed to stop Collector:', error);
      return { success: false, error: error.message };
    }
  }

  async restartCollector(config = {}) {
    console.log('🔄 Restarting Collector service...');
    
    await this.stopCollector();
    
    // Wait a moment before restart
    await new Promise(resolve => setTimeout(resolve, 2000));
    
    return await this.startCollector(config);
  }

  // ========================================
  // 원격 서비스 관리
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

    console.log(`📡 Remote node registered: ${nodeId} (${host}:${port})`);
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

      console.log(`📡 Remote command sent to ${nodeId}: ${action} ${serviceName}`);
      return response;

    } catch (error) {
      console.error(`❌ Remote command failed for ${nodeId}:`, error);
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
      config: service.config
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
    const defaultConfig = {
      database: {
        redis: {
          host: process.env.REDIS_HOST || 'localhost',
          port: process.env.REDIS_PORT || 6379
        },
        influx: {
          url: process.env.INFLUX_URL || 'http://localhost:8086',
          token: process.env.INFLUX_TOKEN,
          org: process.env.INFLUX_ORG || 'pulseone',
          bucket: process.env.INFLUX_BUCKET || 'metrics'
        }
      },
      message_queue: {
        rabbitmq: {
          url: process.env.RABBITMQ_URL || 'amqp://localhost:5672'
        }
      },
      collection: {
        polling_interval: config.polling_interval || 1000,
        batch_size: config.batch_size || 100,
        max_retries: config.max_retries || 3
      },
      drivers: config.drivers || []
    };

    const finalConfig = { ...defaultConfig, ...config };
    
    const configPath = path.join(__dirname, '../../../collector/config/runtime.json');
    await fs.writeFile(configPath, JSON.stringify(finalConfig, null, 2));
    
    console.log(`📝 Collector config generated: ${configPath}`);
    return finalConfig;
  }

  async saveServiceConfig(serviceName, config) {
    this.serviceConfigs.set(serviceName, config);
    
    const configPath = path.join(__dirname, '../config/services.json');
    const allConfigs = Object.fromEntries(this.serviceConfigs);
    await fs.writeFile(configPath, JSON.stringify(allConfigs, null, 2));
    
    console.log(`💾 Service config saved: ${serviceName}`);
  }

  async loadServiceConfigs() {
    try {
      const configPath = path.join(__dirname, '../config/services.json');
      const data = await fs.readFile(configPath, 'utf8');
      const configs = JSON.parse(data);
      
      for (const [serviceName, config] of Object.entries(configs)) {
        this.serviceConfigs.set(serviceName, config);
      }
      
      console.log('📂 Service configs loaded');
    } catch (error) {
      console.log('📂 No existing service configs found, using defaults');
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
        const isHealthy = await this.checkServiceHealth(service);
        health.local_services[serviceName] = {
          status: isHealthy ? 'healthy' : 'unhealthy',
          pid: service.pid,
          uptime: Date.now() - service.startTime.getTime()
        };
      } catch (error) {
        health.local_services[serviceName] = {
          status: 'error',
          error: error.message
        };
        health.overall_status = 'degraded';
      }
    }

    // 원격 노드 헬스 체크
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
    // 프로세스가 살아있는지 확인
    try {
      process.kill(service.pid, 0);
      return true;
    } catch (error) {
      return false;
    }
  }
}

module.exports = new ServiceManager();