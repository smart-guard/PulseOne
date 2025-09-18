// =============================================================================
// backend/lib/services/FaultRecoveryService.js (새 파일)
// 장애 복구 및 자동 재시작 시스템
// =============================================================================

const { spawn, exec } = require('child_process');
const fs = require('fs').promises;
const path = require('path');
const EventEmitter = require('events');
const ConfigManager = require('../config/ConfigManager');
const { getInstance: getCollectorProxy } = require('./CollectorProxyService');

class FaultRecoveryService extends EventEmitter {
  constructor() {
    super();
    this.config = ConfigManager.getInstance();
    this.isEnabled = this.config.getBoolean('FAULT_RECOVERY_ENABLED', true);
    this.checkInterval = this.config.getNumber('FAULT_RECOVERY_CHECK_INTERVAL', 30000); // 30초
    
    // 복구 설정
    this.maxRestartAttempts = this.config.getNumber('FAULT_RECOVERY_MAX_ATTEMPTS', 5);
    this.restartCooldown = this.config.getNumber('FAULT_RECOVERY_COOLDOWN', 60000); // 1분
    this.escalationTimeout = this.config.getNumber('FAULT_RECOVERY_ESCALATION', 300000); // 5분
    
    // 상태 추적
    this.restartAttempts = 0;
    this.lastRestartTime = null;
    this.lastHealthyTime = Date.now();
    this.faultHistory = [];
    this.isRecovering = false;
    
    // 모니터링 인터벌
    this.monitoringInterval = null;
    
    console.log(`🛡️ FaultRecoveryService initialized (enabled: ${this.isEnabled})`);
    
    if (this.isEnabled) {
      this.startMonitoring();
    }
  }

  startMonitoring() {
    if (this.monitoringInterval) {
      clearInterval(this.monitoringInterval);
    }

    this.monitoringInterval = setInterval(async () => {
      await this.performHealthCheck();
    }, this.checkInterval);

    console.log(`🔍 Health monitoring started (interval: ${this.checkInterval}ms)`);
  }

  stopMonitoring() {
    if (this.monitoringInterval) {
      clearInterval(this.monitoringInterval);
      this.monitoringInterval = null;
    }
    console.log('🔍 Health monitoring stopped');
  }

  async performHealthCheck() {
    try {
      const proxy = getCollectorProxy();
      const healthResult = await proxy.healthCheck();
      
      if (healthResult.success) {
        this.onHealthy();
      } else {
        await this.onUnhealthy('Health check failed', healthResult);
      }
      
    } catch (error) {
      await this.onUnhealthy('Health check error', { error: error.message });
    }
  }

  onHealthy() {
    const wasUnhealthy = this.lastHealthyTime < Date.now() - (this.checkInterval * 2);
    
    this.lastHealthyTime = Date.now();
    
    if (wasUnhealthy) {
      console.log('✅ Collector service recovered');
      this.emit('service-recovered');
      
      // 연속 실패 카운터 리셋
      if (this.restartAttempts > 0) {
        console.log(`🔄 Reset restart attempts (was ${this.restartAttempts})`);
        this.restartAttempts = 0;
      }
    }
  }

  async onUnhealthy(reason, details) {
    const unhealthyDuration = Date.now() - this.lastHealthyTime;
    
    console.warn(`⚠️ Collector unhealthy: ${reason} (duration: ${Math.round(unhealthyDuration/1000)}s)`);
    
    this.recordFault(reason, details);
    this.emit('service-unhealthy', { reason, details, duration: unhealthyDuration });
    
    // 복구 중이 아니고 쿨다운 시간이 지났으면 복구 시도
    if (!this.isRecovering && this.shouldAttemptRecovery(unhealthyDuration)) {
      await this.attemptRecovery(reason);
    }
  }

  shouldAttemptRecovery(unhealthyDuration) {
    // 1. 최대 재시작 시도 횟수 확인
    if (this.restartAttempts >= this.maxRestartAttempts) {
      console.error(`❌ Max restart attempts (${this.maxRestartAttempts}) exceeded`);
      return false;
    }

    // 2. 쿨다운 시간 확인
    if (this.lastRestartTime && Date.now() - this.lastRestartTime < this.restartCooldown) {
      return false;
    }

    // 3. 최소 불건전 시간 확인 (잦은 재시작 방지)
    const minUnhealthyTime = Math.max(10000, this.checkInterval * 2); // 최소 10초
    if (unhealthyDuration < minUnhealthyTime) {
      return false;
    }

    return true;
  }

  async attemptRecovery(reason) {
    this.isRecovering = true;
    this.restartAttempts++;
    this.lastRestartTime = Date.now();
    
    console.log(`🔧 Recovery attempt ${this.restartAttempts}/${this.maxRestartAttempts} - ${reason}`);
    this.emit('recovery-started', { attempt: this.restartAttempts, reason });

    try {
      // 복구 단계별 실행
      const recoverySteps = [
        () => this.softRestart(),
        () => this.hardRestart(),
        () => this.systemLevelRecovery(),
        () => this.escalateToOperator()
      ];

      const stepIndex = Math.min(this.restartAttempts - 1, recoverySteps.length - 1);
      const recoveryStep = recoverySteps[stepIndex];
      
      const result = await recoveryStep();
      
      if (result.success) {
        console.log(`✅ Recovery step ${stepIndex + 1} successful`);
        this.emit('recovery-successful', { attempt: this.restartAttempts, step: stepIndex + 1 });
        
        // 복구 성공 후 검증 대기
        await this.waitForRecoveryVerification();
        
      } else {
        console.error(`❌ Recovery step ${stepIndex + 1} failed: ${result.error}`);
        this.emit('recovery-failed', { attempt: this.restartAttempts, step: stepIndex + 1, error: result.error });
      }

    } catch (error) {
      console.error(`❌ Recovery attempt ${this.restartAttempts} failed:`, error.message);
      this.emit('recovery-failed', { attempt: this.restartAttempts, error: error.message });
    }

    this.isRecovering = false;
  }

  // 복구 단계 1: 소프트 재시작 (API 호출)
  async softRestart() {
    console.log('🔄 Attempting soft restart via API...');
    
    try {
      const proxy = getCollectorProxy();
      
      // Collector에 재시작 명령 전송
      await proxy.safeRequest(async () => {
        const response = await proxy.httpClient.post('/api/lifecycle/restart', {
          graceful: true,
          timeout: 30000
        });
        return response;
      }, 1); // 재시도 없이 1회만
      
      return { success: true, method: 'soft_restart' };
      
    } catch (error) {
      return { success: false, error: error.message, method: 'soft_restart' };
    }
  }

  // 복구 단계 2: 하드 재시작 (프로세스 직접 제어)
  async hardRestart() {
    console.log('🔄 Attempting hard restart via process control...');
    
    try {
      // 1. Collector 프로세스 종료
      await this.killCollectorProcess();
      
      // 2. 잠시 대기
      await new Promise(resolve => setTimeout(resolve, 3000));
      
      // 3. Collector 재시작
      const startResult = await this.startCollectorProcess();
      
      if (startResult.success) {
        return { success: true, method: 'hard_restart', pid: startResult.pid };
      } else {
        return { success: false, error: startResult.error, method: 'hard_restart' };
      }
      
    } catch (error) {
      return { success: false, error: error.message, method: 'hard_restart' };
    }
  }

  // 복구 단계 3: 시스템 레벨 복구
  async systemLevelRecovery() {
    console.log('🔄 Attempting system-level recovery...');
    
    try {
      const recovery = {
        cleaned_files: [],
        cleared_ports: [],
        reset_configs: []
      };

      // 1. 임시 파일 정리
      try {
        const tempDirs = ['/tmp/collector*', './logs/collector*', './data/collector*'];
        for (const pattern of tempDirs) {
          const cleaned = await this.cleanupFiles(pattern);
          recovery.cleaned_files.push(...cleaned);
        }
      } catch (error) {
        console.warn('Temp file cleanup failed:', error.message);
      }

      // 2. 포트 정리
      try {
        const collectorPort = this.config.getNumber('COLLECTOR_API_PORT', 8080);
        const portCleared = await this.clearPort(collectorPort);
        if (portCleared) {
          recovery.cleared_ports.push(collectorPort);
        }
      } catch (error) {
        console.warn('Port cleanup failed:', error.message);
      }

      // 3. 설정 파일 백업에서 복구
      try {
        const configRestored = await this.restoreConfigFromBackup();
        if (configRestored) {
          recovery.reset_configs.push('collector.json');
        }
      } catch (error) {
        console.warn('Config restore failed:', error.message);
      }

      // 4. Collector 재시작
      const startResult = await this.startCollectorProcess();
      
      if (startResult.success) {
        return { 
          success: true, 
          method: 'system_recovery',
          pid: startResult.pid,
          recovery_actions: recovery
        };
      } else {
        return { 
          success: false, 
          error: startResult.error, 
          method: 'system_recovery',
          recovery_actions: recovery
        };
      }

    } catch (error) {
      return { success: false, error: error.message, method: 'system_recovery' };
    }
  }

  // 복구 단계 4: 운영자 에스컬레이션
  async escalateToOperator() {
    console.error('🚨 Escalating to operator - automatic recovery failed');
    
    const escalationData = {
      timestamp: new Date().toISOString(),
      service: 'collector',
      fault_history: this.faultHistory.slice(-10),
      restart_attempts: this.restartAttempts,
      system_info: {
        uptime: process.uptime(),
        memory: process.memoryUsage(),
        platform: process.platform
      }
    };

    // 1. 로그 파일에 상세 정보 기록
    await this.writeEscalationReport(escalationData);
    
    // 2. 알림 시스템 (이메일, Slack 등)에 전송
    this.emit('operator-escalation', escalationData);
    
    // 3. 임시 대체 모드 활성화
    await this.enableFallbackMode();
    
    return { 
      success: false, 
      escalated: true, 
      method: 'operator_escalation',
      report_file: path.join('./logs', `escalation-${Date.now()}.json`)
    };
  }

  async killCollectorProcess() {
    return new Promise((resolve) => {
      const isWindows = process.platform === 'win32';
      
      if (isWindows) {
        exec('taskkill /f /im collector.exe', (error) => {
          if (error && !error.message.includes('not found')) {
            console.warn('Windows taskkill warning:', error.message);
          }
          resolve();
        });
      } else {
        exec('pkill -f collector', (error) => {
          if (error && !error.message.includes('no such process')) {
            console.warn('Unix pkill warning:', error.message);
          }
          resolve();
        });
      }
    });
  }

  async startCollectorProcess() {
    try {
      const customPath = this.config.get('COLLECTOR_EXECUTABLE_PATH');
      const isWindows = process.platform === 'win32';
      
      let collectorPath;
      if (customPath && fsSync.existsSync(customPath)) {
        collectorPath = customPath;
      } else {
        collectorPath = isWindows ? 
          path.join(process.cwd(), 'collector.exe') :
          path.join(process.cwd(), 'collector', 'bin', 'collector');
      }
      // 실행 파일 존재 확인
      try {
        await fs.access(collectorPath);
      } catch (error) {
        return { success: false, error: `Collector executable not found: ${collectorPath}` };
      }

      // 프로세스 시작
      const collectorProcess = spawn(collectorPath, [], {
        detached: true,
        stdio: 'ignore'
      });

      if (collectorProcess.pid) {
        collectorProcess.unref();
        
        // 시작 검증 (5초 후)
        await new Promise(resolve => setTimeout(resolve, 5000));
        
        return { success: true, pid: collectorProcess.pid };
      } else {
        return { success: false, error: 'Failed to start collector process' };
      }

    } catch (error) {
      return { success: false, error: error.message };
    }
  }

  async waitForRecoveryVerification() {
    console.log('🔍 Verifying recovery...');
    
    const maxVerificationTime = 60000; // 1분
    const verificationInterval = 5000; // 5초
    const startTime = Date.now();
    
    while (Date.now() - startTime < maxVerificationTime) {
      try {
        const proxy = getCollectorProxy();
        const health = await proxy.healthCheck();
        
        if (health.success) {
          console.log('✅ Recovery verified successfully');
          return true;
        }
        
      } catch (error) {
        // 아직 복구 중일 수 있음
      }
      
      await new Promise(resolve => setTimeout(resolve, verificationInterval));
    }
    
    console.warn('⚠️ Recovery verification timeout');
    return false;
  }

  async cleanupFiles(pattern) {
    // 간단한 파일 정리 (실제 구현에서는 더 안전한 방법 사용)
    return [];
  }

  async clearPort(port) {
    return new Promise((resolve) => {
      const isWindows = process.platform === 'win32';
      
      if (isWindows) {
        exec(`netstat -ano | findstr :${port}`, (error, stdout) => {
          if (stdout) {
            const lines = stdout.trim().split('\n');
            lines.forEach(line => {
              const parts = line.trim().split(/\s+/);
              const pid = parts[parts.length - 1];
              if (pid && !isNaN(pid)) {
                exec(`taskkill /f /pid ${pid}`, () => {});
              }
            });
          }
          resolve(true);
        });
      } else {
        exec(`lsof -t -i:${port}`, (error, stdout) => {
          if (stdout) {
            const pids = stdout.trim().split('\n');
            pids.forEach(pid => {
              if (pid && !isNaN(pid)) {
                exec(`kill -9 ${pid}`, () => {});
              }
            });
          }
          resolve(true);
        });
      }
    });
  }

  async restoreConfigFromBackup() {
    // 설정 파일 백업에서 복구 (구현 필요)
    return false;
  }

  async writeEscalationReport(data) {
    try {
      const logsDir = path.join(__dirname, '../../../logs');
      await fs.mkdir(logsDir, { recursive: true });
      
      const reportFile = path.join(logsDir, `escalation-${Date.now()}.json`);
      await fs.writeFile(reportFile, JSON.stringify(data, null, 2));
      
      console.log(`📄 Escalation report written: ${reportFile}`);
    } catch (error) {
      console.error('Failed to write escalation report:', error.message);
    }
  }

  async enableFallbackMode() {
    // 임시 대체 모드 (Mock 서버 등) 활성화
    console.log('🔄 Enabling fallback mode...');
    this.emit('fallback-mode-enabled');
  }

  recordFault(reason, details) {
    this.faultHistory.push({
      timestamp: Date.now(),
      reason,
      details
    });

    // 최대 100개 기록 유지
    if (this.faultHistory.length > 100) {
      this.faultHistory = this.faultHistory.slice(-100);
    }
  }

  getStatus() {
    return {
      enabled: this.isEnabled,
      restart_attempts: this.restartAttempts,
      max_restart_attempts: this.maxRestartAttempts,
      last_restart_time: this.lastRestartTime,
      last_healthy_time: this.lastHealthyTime,
      is_recovering: this.isRecovering,
      fault_count: this.faultHistory.length,
      recent_faults: this.faultHistory.slice(-5)
    };
  }

  async gracefulShutdown() {
    console.log('🔄 FaultRecoveryService graceful shutdown...');
    this.stopMonitoring();
    console.log('✅ FaultRecoveryService shutdown completed');
  }
}

// 싱글톤 인스턴스
let instance = null;

module.exports = {
  getInstance: () => {
    if (!instance) {
      instance = new FaultRecoveryService();
      
      // Graceful shutdown
      process.on('SIGTERM', () => instance.gracefulShutdown());
      process.on('SIGINT', () => instance.gracefulShutdown());
    }
    return instance;
  },
  FaultRecoveryService
};