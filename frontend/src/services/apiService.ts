// frontend/src/services/apiService.ts
// Backend API와 통신하는 서비스 클래스

export interface ServiceStatus {
  name: string;
  displayName: string;
  status: 'running' | 'stopped' | 'error';
  icon: string;
  controllable: boolean;
  platform: string;
  pid: number | null;
  uptime: string;
  memoryUsage: string;
  cpuUsage: string;
  description: string;
  executablePath?: string;
  exists?: boolean;
}

export interface SystemMetrics {
  dataPointsPerSecond: number;
  avgResponseTime: number;
  dbQueryTime: number;
  cpuUsage: number;
  memoryUsage: number;
  diskUsage: number;
  networkUsage: number;
}

export interface PlatformInfo {
  detected: string;
  architecture: string;
  development: boolean;
  features: {
    serviceManager: string;
    processManager: string;
    packageManager: string;
    autoStart: string;
  };
  paths: {
    root: string;
    collector: string;
    config: string;
    data: string;
    logs: string;
    sqlite: string;
  };
  deployment: {
    current: string;
    recommended: string;
    alternatives: string[];
  };
}

export interface HealthStatus {
  overall: 'healthy' | 'degraded' | 'critical' | 'error';
  platform: string;
  architecture: string;
  development: boolean;
  services: {
    backend: {
      running: boolean;
      pid: number | null;
      platform: string;
      essential: boolean;
    };
    collector: {
      running: boolean;
      pid: number | null;
      platform: string;
      essential: boolean;
      available: boolean;
    };
  };
  checks: {
    collector_executable: boolean;
    database_file: boolean;
    config_directory: boolean;
    logs_directory: boolean;
    data_directory: boolean;
    systemd_available?: boolean;
    docker_available?: boolean;
    windows_services?: boolean;
  };
  timestamp: string;
}

class ApiService {
  private baseUrl: string;

  constructor(baseUrl: string = 'http://localhost:3000/api') {
    this.baseUrl = baseUrl;
  }

  // ========================================
  // 🔧 HTTP 유틸리티 메소드들
  // ========================================

  private async request<T>(
    endpoint: string, 
    options: RequestInit = {}
  ): Promise<T> {
    const url = `${this.baseUrl}${endpoint}`;
    
    const config: RequestInit = {
      headers: {
        'Content-Type': 'application/json',
        ...options.headers,
      },
      ...options,
    };

    try {
      const response = await fetch(url, config);
      
      if (!response.ok) {
        const errorData = await response.json().catch(() => ({}));
        throw new Error(
          errorData.error || `HTTP ${response.status}: ${response.statusText}`
        );
      }

      return await response.json();
    } catch (error) {
      console.error(`API Request failed: ${endpoint}`, error);
      throw error;
    }
  }

  private async get<T>(endpoint: string): Promise<T> {
    return this.request<T>(endpoint, { method: 'GET' });
  }

  private async post<T>(endpoint: string, data?: any): Promise<T> {
    return this.request<T>(endpoint, {
      method: 'POST',
      body: data ? JSON.stringify(data) : undefined,
    });
  }

  // ========================================
  // 🎯 서비스 관리 API
  // ========================================

  /**
   * 모든 서비스 상태 조회
   */
  async getServices(): Promise<{
    success: boolean;
    data: ServiceStatus[];
    summary: {
      total: number;
      running: number;
      stopped: number;
      unknown: number;
    };
    platform: {
      type: string;
      architecture: string;
      deployment: string;
      development: boolean;
    };
    paths: any;
    timestamp: string;
  }> {
    return this.get('/services');
  }

  /**
   * 특정 서비스 상세 정보 조회
   */
  async getService(serviceName: string): Promise<{
    success: boolean;
    data: ServiceStatus;
  }> {
    return this.get(`/services/${serviceName}`);
  }

  /**
   * Collector 서비스 시작
   */
  async startCollector(): Promise<{
    success: boolean;
    message?: string;
    error?: string;
    pid?: number;
    platform?: string;
    suggestion?: string;
    buildCommand?: string;
    development?: boolean;
  }> {
    return this.post('/services/collector/start');
  }

  /**
   * Collector 서비스 중지
   */
  async stopCollector(): Promise<{
    success: boolean;
    message?: string;
    error?: string;
    platform?: string;
  }> {
    return this.post('/services/collector/stop');
  }

  /**
   * Collector 서비스 재시작
   */
  async restartCollector(): Promise<{
    success: boolean;
    message?: string;
    error?: string;
    pid?: number;
    platform?: string;
  }> {
    return this.post('/services/collector/restart');
  }

  // ========================================
  // 📊 시스템 정보 API
  // ========================================

  /**
   * 플랫폼 정보 조회
   */
  async getPlatformInfo(): Promise<{
    success: boolean;
    data: PlatformInfo;
  }> {
    return this.get('/services/platform/info');
  }

  /**
   * 시스템 정보 조회
   */
  async getSystemInfo(): Promise<{
    success: boolean;
    data: {
      platform: any;
      runtime: any;
      memory: any;
      system: any;
      pulseone: any;
      deployment: any;
    };
  }> {
    return this.get('/services/system/info');
  }

  /**
   * 헬스 체크
   */
  async getHealthCheck(): Promise<{
    success: boolean;
    health: HealthStatus;
    deployment: {
      type: string;
      containerized: boolean;
      platform: string;
      architecture: string;
      nodeVersion: string;
    };
  }> {
    return this.get('/services/health/check');
  }

  /**
   * 실행 중인 프로세스 조회
   */
  async getProcesses(): Promise<{
    success: boolean;
    data: {
      processes: any[];
      summary: {
        total: number;
        backend: number;
        collector: number;
      };
      platform: string;
      architecture: string;
    };
  }> {
    return this.get('/services/processes');
  }

  // ========================================
  // 🔄 실시간 모니터링
  // ========================================

  /**
   * 서비스 상태 폴링 (주기적 업데이트용)
   */
  startServicePolling(
    callback: (services: ServiceStatus[]) => void,
    interval: number = 10000
  ): number {
    const pollServices = async () => {
      try {
        const response = await this.getServices();
        if (response.success) {
          callback(response.data);
        }
      } catch (error) {
        console.error('Service polling error:', error);
      }
    };

    // 즉시 한 번 실행
    pollServices();

    // 주기적 실행
    return window.setInterval(pollServices, interval);
  }

  /**
   * 폴링 중지
   */
  stopPolling(intervalId: number): void {
    clearInterval(intervalId);
  }

  // ========================================
  // 🎮 서비스 제어 헬퍼 메소드들
  // ========================================

  /**
   * 서비스 제어 (시작/중지/재시작)
   */
  async controlService(
    serviceName: string, 
    action: 'start' | 'stop' | 'restart'
  ): Promise<{ success: boolean; message?: string; error?: string }> {
    if (serviceName === 'collector') {
      switch (action) {
        case 'start':
          return this.startCollector();
        case 'stop':
          return this.stopCollector();
        case 'restart':
          return this.restartCollector();
        default:
          throw new Error(`Invalid action: ${action}`);
      }
    } else {
      throw new Error(`Service ${serviceName} is not controllable`);
    }
  }

  /**
   * 서비스 상태 확인 (단순)
   */
  async isServiceRunning(serviceName: string): Promise<boolean> {
    try {
      const response = await this.getService(serviceName);
      return response.success && response.data.status === 'running';
    } catch (error) {
      console.error(`Error checking service ${serviceName}:`, error);
      return false;
    }
  }

  // ========================================
  // 📈 통계 및 메트릭 시뮬레이션
  // ========================================

  /**
   * 시스템 메트릭 생성 (실제 환경에서는 실제 데이터)
   */
  generateSystemMetrics(): SystemMetrics {
    return {
      dataPointsPerSecond: Math.floor(Math.random() * 1000) + 2000,
      avgResponseTime: Math.floor(Math.random() * 100) + 100,
      dbQueryTime: Math.floor(Math.random() * 50) + 10,
      cpuUsage: Math.floor(Math.random() * 100),
      memoryUsage: Math.floor(Math.random() * 100),
      diskUsage: Math.floor(Math.random() * 100),
      networkUsage: Math.floor(Math.random() * 100),
    };
  }
}

// 싱글턴 인스턴스 생성
export const apiService = new ApiService();

// Default export
export default apiService;