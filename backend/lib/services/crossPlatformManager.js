// backend/lib/services/crossPlatformManager.js - 완성본
// Windows, Linux, macOS, AWS 모두 지원 + Redis 제어 기능 포함

const os = require('os');
const path = require('path');
const { spawn, exec } = require('child_process');
const fs = require('fs').promises;
const fsSync = require('fs');

// ConfigManager 연동
const config = require('../config/ConfigManager');

class CrossPlatformManager {
  constructor() {
    this.platform = os.platform();
    this.architecture = os.arch();
    this.isWindows = this.platform === 'win32';
    this.isLinux = this.platform === 'linux';
    this.isMac = this.platform === 'darwin';
    this.isDevelopment = this.detectDevelopmentEnvironment();
    
    // 플랫폼별 설정 초기화
    this.paths = this.initializePaths();
    this.commands = this.initializeCommands();
    
    console.log(`🎯 CrossPlatformManager initialized for ${this.platform} (${this.architecture})`);
    console.log(`📂 Development mode: ${this.isDevelopment}`);
  }

  // ========================================
  // 🔍 환경 감지
  // ========================================

  detectDevelopmentEnvironment() {
    // 여러 방법으로 개발 환경 감지
    const indicators = [
      process.env.NODE_ENV === 'development',
      process.env.ENV_STAGE === 'dev',
      process.cwd().includes('/app'),
      process.cwd().includes('\\app'),
      process.env.DOCKER_CONTAINER === 'true'
    ];

    // Docker 환경 감지
    try {
      fsSync.accessSync('/.dockerenv');
      indicators.push(true);
    } catch (e) {
      // Docker 환경 아님
    }

    return indicators.some(Boolean);
  }

  // ========================================
  // 📁 플랫폼별 경로 설정 (ConfigManager 통합)
  // ========================================

  initializePaths() {
    // 1. ConfigManager에서 커스텀 경로 확인
    const customCollectorPath = config.get('COLLECTOR_EXECUTABLE_PATH');
    const customRedisPath = config.get('REDIS_EXECUTABLE_PATH');
    
    const basePaths = {
      development: {
        win32: {
          root: process.cwd(),
          collector: customCollectorPath || path.join(process.cwd(), 'collector', 'bin', 'collector.exe'),
          redis: customRedisPath || path.join(process.cwd(), 'redis-server.exe'),
          config: path.join(process.cwd(), 'config'),
          data: path.join(process.cwd(), 'data'),
          logs: path.join(process.cwd(), 'logs'),
          sqlite: path.join(process.cwd(), 'data', 'pulseone.db'),
          separator: '\\'
        },
        linux: {
          root: process.cwd(),
          collector: customCollectorPath || path.join(process.cwd(), 'collector', 'bin', 'collector'),
          redis: customRedisPath || '/usr/bin/redis-server',
          config: path.join(process.cwd(), 'config'),
          data: path.join(process.cwd(), 'data'),
          logs: path.join(process.cwd(), 'logs'),
          sqlite: path.join(process.cwd(), 'data', 'pulseone.db'),
          separator: '/'
        },
        darwin: {
          root: process.cwd(),
          collector: customCollectorPath || path.join(process.cwd(), 'collector', 'bin', 'collector'),
          redis: customRedisPath || '/usr/local/bin/redis-server',
          config: path.join(process.cwd(), 'config'),
          data: path.join(process.cwd(), 'data'),
          logs: path.join(process.cwd(), 'logs'),
          sqlite: path.join(process.cwd(), 'data', 'pulseone.db'),
          separator: '/'
        }
      },
      production: {
        win32: {
          root: 'C:\\PulseOne',
          collector: customCollectorPath || 'C:\\PulseOne\\collector\\bin\\collector.exe',
          redis: customRedisPath || 'C:\\PulseOne\\redis-server.exe',
          config: 'C:\\PulseOne\\config',
          data: 'C:\\PulseOne\\data',
          logs: 'C:\\PulseOne\\logs',
          sqlite: 'C:\\PulseOne\\data\\pulseone.db',
          separator: '\\'
        },
        linux: {
          root: '/opt/pulseone',
          collector: customCollectorPath || '/opt/pulseone/collector/bin/collector',
          redis: customRedisPath || '/usr/bin/redis-server',
          config: '/opt/pulseone/config',
          data: '/opt/pulseone/data',
          logs: '/var/log/pulseone',
          sqlite: '/opt/pulseone/data/pulseone.db',
          separator: '/'
        },
        darwin: {
          root: '/Applications/PulseOne.app/Contents',
          collector: customCollectorPath || '/Applications/PulseOne.app/Contents/collector/bin/collector',
          redis: customRedisPath || '/usr/local/bin/redis-server',
          config: '/Users/Shared/PulseOne/config',
          data: '/Users/Shared/PulseOne/data',
          logs: '/Users/Shared/PulseOne/logs',
          sqlite: '/Users/Shared/PulseOne/data/pulseone.db',
          separator: '/'
        }
      }
    };

    const environment = this.isDevelopment ? 'development' : 'production';
    return basePaths[environment][this.platform] || basePaths.development.linux;
  }

  // ========================================
  // 🔧 플랫폼별 명령어 설정
  // ========================================

  initializeCommands() {
    if (this.isWindows) {
      return {
        processFind: 'wmic process where "name=\'collector.exe\' or name=\'redis-server.exe\' or (name=\'node.exe\' and CommandLine like \'%app.js%\')" get ProcessId,Name,CommandLine,CreationDate /format:csv',
        processKill: (pid) => `taskkill /PID ${pid} /F`,
        serviceList: 'sc query type=service state=all | findstr "PulseOne"',
        systemInfo: 'wmic OS get TotalVisibleMemorySize,FreePhysicalMemory /value',
        pathExists: (path) => `if exist "${path}" echo "EXISTS" else echo "NOT_EXISTS"`,
        redisCliShutdown: 'redis-cli.exe shutdown'
      };
    } else {
      return {
        processFind: 'ps aux | grep -E "(collector|redis-server|node.*app\.js)" | grep -v grep',
        processKill: (pid) => `kill -TERM ${pid}`,
        serviceList: 'systemctl list-units --type=service | grep pulseone',
        systemInfo: 'free -h && df -h',
        pathExists: (path) => `[ -f "${path}" ] && echo "EXISTS" || echo "NOT_EXISTS"`,
        redisCliShutdown: 'redis-cli shutdown'
      };
    }
  }

  // ========================================
  // 🔍 프로세스 조회 (Redis 추가)
  // ========================================

  async getRunningProcesses() {
    try {
      if (this.isWindows) {
        return await this.getWindowsProcesses();
      } else {
        return await this.getUnixProcesses();
      }
    } catch (error) {
      console.error('Error getting processes:', error);
      return { backend: [], collector: [], redis: [] };
    }
  }

  async getWindowsProcesses() {
    try {
      const { stdout } = await this.execCommand(this.commands.processFind);
      const lines = stdout.split('\n').filter(line => line.trim() && !line.includes('Node,Name'));
      
      const processes = { backend: [], collector: [], redis: [] };
      
      lines.forEach(line => {
        const parts = line.split(',');
        if (parts.length >= 5) {
          const [, commandLine, creationDate, name, pid] = parts;
          
          const processInfo = {
            pid: parseInt(pid),
            name: name.trim(),
            commandLine: commandLine.trim(),
            startTime: creationDate ? new Date(creationDate) : null,
            platform: 'windows'
          };

          if (name.includes('node.exe') && commandLine.includes('app.js')) {
            processes.backend.push(processInfo);
          } else if (name.includes('collector.exe')) {
            processes.collector.push(processInfo);
          } else if (name.includes('redis-server.exe')) {
            processes.redis.push(processInfo);
          }
        }
      });

      return processes;
    } catch (error) {
      console.error('Error getting Windows processes:', error);
      return { backend: [], collector: [], redis: [] };
    }
  }

  async getUnixProcesses() {
    try {
      const { stdout } = await this.execCommand(this.commands.processFind);
      const lines = stdout.split('\n').filter(line => line.trim());
      
      const processes = { backend: [], collector: [], redis: [] };
      
      lines.forEach(line => {
        const parts = line.trim().split(/\s+/);
        if (parts.length >= 11) {
          const pid = parseInt(parts[1]);
          const command = parts.slice(10).join(' ');
          
          const processInfo = {
            pid: pid,
            name: parts[10],
            commandLine: command,
            user: parts[0],
            cpu: parts[2],
            memory: parts[3],
            platform: this.platform
          };

          if (command.includes('node') && command.includes('app.js')) {
            processes.backend.push(processInfo);
          } else if (command.includes('collector')) {
            processes.collector.push(processInfo);
          } else if (command.includes('redis-server')) {
            processes.redis.push(processInfo);
          }
        }
      });

      return processes;
    } catch (error) {
      console.error('Error getting Unix processes:', error);
      return { backend: [], collector: [], redis: [] };
    }
  }

  // ========================================
  // 🎮 Collector 서비스 제어
  // ========================================

  async startCollector() {
    console.log(`🚀 Starting Collector on ${this.platform}...`);
    
    try {
      // 1. 실행파일 존재 확인
      const collectorExists = await this.fileExists(this.paths.collector);
      if (!collectorExists) {
        console.warn(`⚠️ Collector executable not found: ${this.paths.collector}`);
        
        if (this.isDevelopment) {
          return {
            success: false,
            error: `Collector executable not found: ${this.paths.collector}`,
            suggestion: 'Build the Collector project first',
            buildCommand: this.isWindows ? 'cd collector && make' : 'cd collector && make debug',
            platform: this.platform,
            development: true
          };
        } else {
          return {
            success: false,
            error: `Collector not installed: ${this.paths.collector}`,
            suggestion: 'Install PulseOne Collector package',
            platform: this.platform,
            development: false
          };
        }
      }

      // 2. 이미 실행 중인지 확인
      const processes = await this.getRunningProcesses();
      if (processes.collector.length > 0) {
        return {
          success: false,
          error: `Collector already running (PID: ${processes.collector[0].pid})`,
          platform: this.platform
        };
      }

      // 3. Collector 시작
      const startResult = await this.spawnCollector();
      
      // 4. 시작 확인 (3초 대기)
      await this.sleep(3000);
      
      const newProcesses = await this.getRunningProcesses();
      const newCollector = newProcesses.collector[0];

      if (newCollector) {
        return {
          success: true,
          message: `Collector started successfully on ${this.platform}`,
          pid: newCollector.pid,
          platform: this.platform,
          executable: this.paths.collector
        };
      } else {
        return {
          success: false,
          error: 'Collector process failed to start',
          platform: this.platform,
          suggestion: 'Check collector logs for startup errors'
        };
      }

    } catch (error) {
      console.error('❌ Failed to start Collector:', error);
      return {
        success: false,
        error: error.message,
        platform: this.platform
      };
    }
  }

  async spawnCollector() {
    const collectorDir = path.dirname(this.paths.collector);
    
    if (this.isWindows) {
      return spawn(this.paths.collector, [], {
        cwd: collectorDir,
        detached: true,
        stdio: 'ignore'
      });
    } else {
      return spawn(this.paths.collector, [], {
        cwd: collectorDir,
        detached: true,
        stdio: 'ignore',
        env: {
          ...process.env,
          LD_LIBRARY_PATH: '/usr/local/lib:/usr/lib',
          PATH: process.env.PATH + ':/usr/local/bin'
        }
      });
    }
  }

  async stopCollector() {
    console.log(`🛑 Stopping Collector on ${this.platform}...`);
    
    try {
      const processes = await this.getRunningProcesses();
      const runningCollector = processes.collector[0];

      if (!runningCollector) {
        return {
          success: false,
          error: 'Collector is not running',
          platform: this.platform
        };
      }

      // 프로세스 종료
      await this.execCommand(this.commands.processKill(runningCollector.pid));
      
      // 종료 확인 (3초 대기)
      await this.sleep(3000);
      
      const newProcesses = await this.getRunningProcesses();
      const stillRunning = newProcesses.collector.find(p => p.pid === runningCollector.pid);

      if (!stillRunning) {
        return {
          success: true,
          message: `Collector stopped successfully on ${this.platform}`,
          platform: this.platform
        };
      } else {
        return {
          success: false,
          error: 'Failed to stop Collector process',
          platform: this.platform
        };
      }

    } catch (error) {
      console.error('❌ Failed to stop Collector:', error);
      return {
        success: false,
        error: error.message,
        platform: this.platform
      };
    }
  }

  async restartCollector() {
    console.log(`🔄 Restarting Collector on ${this.platform}...`);
    
    const stopResult = await this.stopCollector();
    if (!stopResult.success && !stopResult.error.includes('not running')) {
      return stopResult;
    }

    await this.sleep(2000);
    
    return await this.startCollector();
  }

  // ========================================
  // 🔴 Redis 서비스 제어
  // ========================================

  async startRedis() {
    console.log(`🚀 Starting Redis on ${this.platform}...`);
    
    try {
      // 1. Redis 경로 확인
      const redisPath = this.paths.redis;
      const redisExists = await this.fileExists(redisPath);
      
      if (!redisExists) {
        return {
          success: false,
          error: `Redis not found at: ${redisPath}`,
          suggestion: this.isWindows ? 
            'Download Redis for Windows from GitHub' : 
            'Install Redis using package manager (apt/yum/brew)',
          platform: this.platform
        };
      }

      // 2. 이미 실행 중인지 확인
      const processes = await this.getRunningProcesses();
      if (processes.redis.length > 0) {
        return {
          success: false,
          error: `Redis already running (PID: ${processes.redis[0].pid})`,
          platform: this.platform,
          port: 6379
        };
      }

      // 3. Redis 시작
      let redisProcess;
      const redisConfig = config.getRedisConfig();
      const port = redisConfig.port || 6379;
      
      if (this.isWindows) {
        // Windows: redis-server.exe 실행
        redisProcess = spawn(redisPath, [
          '--port', port.toString(),
          '--maxmemory', '512mb',
          '--maxmemory-policy', 'allkeys-lru'
        ], {
          detached: true,
          stdio: 'ignore'
        });
      } else {
        // Linux/macOS: redis-server 실행
        redisProcess = spawn(redisPath, [
          '--port', port.toString(),
          '--daemonize', 'yes',
          '--maxmemory', '512mb',
          '--maxmemory-policy', 'allkeys-lru'
        ], {
          detached: true,
          stdio: 'ignore'
        });
      }

      // 4. 시작 확인
      await this.sleep(2000);
      
      const newProcesses = await this.getRunningProcesses();
      const newRedis = newProcesses.redis[0];

      if (newRedis) {
        return {
          success: true,
          message: `Redis started successfully on ${this.platform}`,
          pid: newRedis.pid,
          platform: this.platform,
          port: port
        };
      } else {
        return {
          success: false,
          error: 'Redis process failed to start',
          platform: this.platform,
          suggestion: 'Check Redis logs or try manual start'
        };
      }

    } catch (error) {
      console.error('❌ Failed to start Redis:', error);
      return {
        success: false,
        error: error.message,
        platform: this.platform
      };
    }
  }

  async stopRedis() {
    console.log(`🛑 Stopping Redis on ${this.platform}...`);
    
    try {
      const processes = await this.getRunningProcesses();
      const runningRedis = processes.redis[0];

      if (!runningRedis) {
        return {
          success: false,
          error: 'Redis is not running',
          platform: this.platform
        };
      }

      // Redis 정상 종료 시도 (redis-cli shutdown)
      try {
        await this.execCommand(this.commands.redisCliShutdown);
        await this.sleep(2000);
      } catch (cliError) {
        console.warn('Redis CLI shutdown failed, using force kill');
        // CLI 실패시 강제 종료
        await this.execCommand(this.commands.processKill(runningRedis.pid));
      }

      // 종료 확인
      await this.sleep(1000);
      const newProcesses = await this.getRunningProcesses();
      const stillRunning = newProcesses.redis.find(p => p.pid === runningRedis.pid);

      if (!stillRunning) {
        return {
          success: true,
          message: `Redis stopped successfully on ${this.platform}`,
          platform: this.platform
        };
      } else {
        // 강제 종료 재시도
        await this.execCommand(this.commands.processKill(runningRedis.pid));
        await this.sleep(1000);
        
        return {
          success: true,
          message: `Redis force stopped on ${this.platform}`,
          platform: this.platform
        };
      }

    } catch (error) {
      console.error('❌ Failed to stop Redis:', error);
      return {
        success: false,
        error: error.message,
        platform: this.platform
      };
    }
  }

  async restartRedis() {
    console.log(`🔄 Restarting Redis on ${this.platform}...`);
    
    const stopResult = await this.stopRedis();
    if (!stopResult.success && !stopResult.error.includes('not running')) {
      return stopResult;
    }

    await this.sleep(2000);
    
    return await this.startRedis();
  }

  async getRedisStatus() {
    try {
      const processes = await this.getRunningProcesses();
      const redisProcess = processes.redis[0];
      
      if (redisProcess) {
        // Redis ping 테스트
        let isResponding = false;
        try {
          const { stdout } = await this.execCommand('redis-cli ping');
          isResponding = stdout.trim() === 'PONG';
        } catch {
          isResponding = false;
        }

        return {
          running: true,
          pid: redisProcess.pid,
          responding: isResponding,
          memory: redisProcess.memory,
          cpu: redisProcess.cpu,
          uptime: this.calculateUptime(redisProcess.startTime),
          port: config.getRedisConfig().port || 6379,
          platform: this.platform
        };
      } else {
        return {
          running: false,
          responding: false,
          platform: this.platform
        };
      }
    } catch (error) {
      return {
        running: false,
        error: error.message,
        platform: this.platform
      };
    }
  }

  // ========================================
  // 📊 서비스 상태 조회 (Redis 포함)
  // ========================================

  async getServicesForAPI() {
    const processes = await this.getRunningProcesses();
    
    const services = [
      {
        name: 'backend',
        displayName: 'Backend API',
        icon: 'fas fa-server',
        description: `REST API 서버 (${this.platform})`,
        controllable: false,
        status: processes.backend.length > 0 ? 'running' : 'stopped',
        pid: processes.backend[0]?.pid || null,
        platform: this.platform,
        executable: this.isWindows ? 'node.exe' : 'node',
        uptime: processes.backend[0] ? this.calculateUptime(processes.backend[0].startTime) : 'N/A',
        memoryUsage: processes.backend[0]?.memory || 'N/A',
        cpuUsage: processes.backend[0]?.cpu || 'N/A'
      },
      {
        name: 'collector',
        displayName: 'Data Collector',
        icon: 'fas fa-download',
        description: `C++ 데이터 수집 서비스 (${this.platform})`,
        controllable: true,
        status: processes.collector.length > 0 ? 'running' : 'stopped',
        pid: processes.collector[0]?.pid || null,
        platform: this.platform,
        executable: path.basename(this.paths.collector),
        uptime: processes.collector[0] ? this.calculateUptime(processes.collector[0].startTime) : 'N/A',
        memoryUsage: processes.collector[0]?.memory || 'N/A',
        cpuUsage: processes.collector[0]?.cpu || 'N/A',
        executablePath: this.paths.collector,
        exists: await this.fileExists(this.paths.collector)
      },
      {
        name: 'redis',
        displayName: 'Redis Cache',
        icon: 'fas fa-database',
        description: `인메모리 캐시 서버 (${this.platform})`,
        controllable: true,
        status: processes.redis.length > 0 ? 'running' : 'stopped',
        pid: processes.redis[0]?.pid || null,
        platform: this.platform,
        executable: path.basename(this.paths.redis),
        uptime: processes.redis[0] ? this.calculateUptime(processes.redis[0].startTime) : 'N/A',
        memoryUsage: processes.redis[0]?.memory || 'N/A',
        cpuUsage: processes.redis[0]?.cpu || 'N/A',
        executablePath: this.paths.redis,
        exists: await this.fileExists(this.paths.redis),
        port: config.getRedisConfig().port || 6379
      }
    ];

    const summary = services.reduce((acc, service) => {
      acc.total++;
      if (service.status === 'running') acc.running++;
      else if (service.status === 'stopped') acc.stopped++;
      return acc;
    }, { total: 0, running: 0, stopped: 0, unknown: 0 });

    return {
      success: true,
      data: services,
      summary,
      platform: {
        type: this.platform,
        architecture: this.architecture,
        deployment: this.getDeploymentType(),
        development: this.isDevelopment
      },
      paths: this.paths,
      timestamp: new Date().toISOString()
    };
  }

  // ========================================
  // 🏥 헬스 체크
  // ========================================

  async performHealthCheck() {
    const health = {
      overall: 'healthy',
      platform: this.platform,
      architecture: this.architecture,
      development: this.isDevelopment,
      services: {},
      system: await this.getSystemInfo(),
      checks: {
        collector_executable: await this.fileExists(this.paths.collector),
        redis_executable: await this.fileExists(this.paths.redis),
        database_file: await this.fileExists(this.paths.sqlite),
        config_directory: await this.directoryExists(this.paths.config),
        logs_directory: await this.directoryExists(this.paths.logs),
        data_directory: await this.directoryExists(this.paths.data)
      },
      timestamp: new Date().toISOString()
    };

    try {
      // 프로세스 상태 확인
      const processes = await this.getRunningProcesses();
      
      health.services.backend = {
        running: processes.backend.length > 0,
        pid: processes.backend[0]?.pid || null,
        platform: this.platform,
        essential: true
      };
      
      health.services.collector = {
        running: processes.collector.length > 0,
        pid: processes.collector[0]?.pid || null,
        platform: this.platform,
        essential: false,
        available: health.checks.collector_executable
      };

      health.services.redis = {
        running: processes.redis.length > 0,
        pid: processes.redis[0]?.pid || null,
        platform: this.platform,
        essential: false,
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

      // 플랫폼별 추가 체크
      if (this.isLinux) {
        health.checks.systemd_available = await this.commandExists('systemctl');
        health.checks.docker_available = await this.commandExists('docker');
      } else if (this.isWindows) {
        health.checks.windows_services = await this.commandExists('sc');
      }

    } catch (error) {
      health.overall = 'error';
      health.error = error.message;
    }

    return health;
  }

  // ========================================
  // 🖥️ 시스템 정보
  // ========================================

  async getSystemInfo() {
    return {
      platform: {
        type: this.platform,
        architecture: this.architecture,
        hostname: os.hostname(),
        release: os.release(),
        version: os.version?.() || 'Unknown',
        deployment: this.getDeploymentType(),
        development: this.isDevelopment
      },
      runtime: {
        nodeVersion: process.version,
        pid: process.pid,
        uptime: `${Math.round(process.uptime() / 60)}분`,
        workingDirectory: process.cwd(),
        environment: process.env.NODE_ENV || 'development'
      },
      memory: {
        total: `${Math.round(os.totalmem() / 1024 / 1024 / 1024)}GB`,
        free: `${Math.round(os.freemem() / 1024 / 1024 / 1024)}GB`,
        used: `${Math.round((os.totalmem() - os.freemem()) / 1024 / 1024 / 1024)}GB`,
        processMemory: this.formatBytes(process.memoryUsage().rss)
      },
      system: {
        cpuCores: os.cpus().length,
        cpuModel: os.cpus()[0]?.model || 'Unknown',
        systemUptime: `${Math.round(os.uptime() / 3600)}시간`,
        loadAverage: os.loadavg()
      },
      pulseone: {
        installationPath: this.paths.root,
        configPath: this.paths.config,
        dataPath: this.paths.data,
        logsPath: this.paths.logs,
        sqlitePath: this.paths.sqlite,
        collectorPath: this.paths.collector,
        redisPath: this.paths.redis
      }
    };
  }

  // ========================================
  // 🛠️ 유틸리티 메소드들
  // ========================================

  async execCommand(command) {
    return new Promise((resolve, reject) => {
      exec(command, { encoding: 'utf8', timeout: 10000 }, (error, stdout, stderr) => {
        if (error) {
          reject(error);
        } else {
          resolve({ stdout, stderr });
        }
      });
    });
  }

  async fileExists(filePath) {
    try {
      const stats = await fs.stat(filePath);
      return stats.isFile();
    } catch {
      return false;
    }
  }

  async directoryExists(dirPath) {
    try {
      const stats = await fs.stat(dirPath);
      return stats.isDirectory();
    } catch {
      return false;
    }
  }

  async commandExists(command) {
    const testCommand = this.isWindows ? `where ${command}` : `which ${command}`;
    try {
      await this.execCommand(testCommand);
      return true;
    } catch {
      return false;
    }
  }

  sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  formatBytes(bytes) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  }

  calculateUptime(startTime) {
    if (!startTime) return 'N/A';
    
    const uptimeMs = Date.now() - new Date(startTime).getTime();
    const seconds = Math.floor(uptimeMs / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    if (days > 0) {
      return `${days}d ${hours % 24}h ${minutes % 60}m`;
    } else if (hours > 0) {
      return `${hours}h ${minutes % 60}m`;
    } else if (minutes > 0) {
      return `${minutes}m ${seconds % 60}s`;
    } else {
      return `${seconds}s`;
    }
  }

  getDeploymentType() {
    if (this.isDevelopment) {
      return this.isWindows ? 'Windows Development' : 
             this.isLinux ? 'Linux Development / Docker' : 
             this.isMac ? 'macOS Development' : 'Development';
    } else {
      return this.isWindows ? 'Windows Native Package' :
             this.isLinux ? 'Linux Package / Docker / AWS' :
             this.isMac ? 'macOS Application' : 'Cross Platform';
    }
  }

  // ========================================
  // 📋 플랫폼 정보 제공
  // ========================================

  getPlatformInfo() {
    return {
      detected: this.platform,
      architecture: this.architecture,
      development: this.isDevelopment,
      features: {
        serviceManager: this.platform === 'win32' ? 'Windows Services' : 'systemd',
        processManager: this.platform === 'win32' ? 'Task Manager' : 'ps/htop',
        packageManager: this.platform === 'win32' ? 'MSI/NSIS' : 
                       this.platform === 'linux' ? 'apt/yum/dnf' : 'Homebrew',
        autoStart: this.platform === 'win32' ? 'Windows Services' : 
                  this.platform === 'linux' ? 'systemd units' : 'launchd'
      },
      paths: this.paths,
      deployment: {
        current: this.getDeploymentType(),
        recommended: this.platform === 'win32' ? 'Native Windows Package' :
                    this.platform === 'linux' ? 'Docker or Native Package' :
                    'Native Application Bundle',
        alternatives: ['Docker', 'Manual Installation', 'Cloud Deployment']
      }
    };
  }
}

// 싱글턴 인스턴스 생성
const crossPlatformManager = new CrossPlatformManager();

module.exports = crossPlatformManager;