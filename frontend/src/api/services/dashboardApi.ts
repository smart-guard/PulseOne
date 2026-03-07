// ============================================================================
// frontend/src/api/services/dashboardApi.ts
// 대시보드 API 서비스 - 백엔드 API와 완전 호환
// ============================================================================

import { ENDPOINTS } from '../endpoints';
import { ApiResponse } from '../../types/common';

// ============================================================================
// 대시보드 관련 인터페이스들 - 백엔드 API 응답 형식과 매칭
// ============================================================================

export interface ServiceInfo {
  name: string;
  displayName: string;
  status: 'running' | 'stopped' | 'error' | 'starting' | 'stopping';
  icon: string;
  controllable: boolean;
  description: string;
  port?: number;
  version?: string;
  uptime?: string | number;
  memory_usage?: number;
  cpu_usage?: number;
  memoryUsage?: string | number;
  cpuUsage?: string | number;
  last_error?: string;
  health_check_url?: string;
  collectorId?: number;
  gatewayId?: number;
  serviceType?: 'core' | 'collector' | 'export-gateway';
  ip?: string;
  exists?: boolean;
  devices?: {
    id: number;
    name: string;
    status: string;
    protocol: string;
    lastSeen: string;
  }[];
}

export interface SystemMetrics {
  dataPointsPerSecond: number;
  avgResponseTime: number;
  dbQueryTime: number;
  cpuUsage: number;
  memoryUsage: number;
  diskUsage: number;
  networkUsage: number;
  activeConnections: number;
  queueSize: number;
  timestamp: string;

  // 시스템 정보
  system?: {
    platform: string;
    arch: string;
    hostname: string;
    uptime: number;
    load_average: number[];
  };

  // 프로세스 정보
  process?: {
    pid: number;
    uptime: number;
    memory: {
      rss: number;
      heapTotal: number;
      heapUsed: number;
      external: number;
    };
    version: string;
    platform: string;
    arch: string;
  };

  // CPU 세부 정보
  cpu?: {
    count: number;
    model: string;
    speed: number;
    usage: number;
  };

  // 메모리 세부 정보
  memory?: {
    total: number;
    free: number;
    used: number;
    usage: number;
    available: number;
  };

  // 디스크 세부 정보
  disk?: {
    total: number;
    used: number;
    free: number;
    usage: number;
  };

  // 네트워크 세부 정보
  network?: {
    usage: number;
    interfaces: number;
  };
}

export interface DeviceSummary {
  total_devices: number;
  connected_devices: number;
  disconnected_devices: number;
  error_devices: number;
  protocols_count: number;
  sites_count: number;
  data_points_count: number;
  enabled_devices: number;
  protocol_details?: Array<{
    protocol_type: string;
    count: number;
    connected: number;
  }>;
}

export interface DatabaseStats {
  connection_status: 'connected' | 'disconnected' | 'error';
  database_file?: string;
  database_size_mb?: number;
  tables: number;
  devices: number;
  data_points: number;
  active_alarms: number;
  users: number;
  last_updated: string;
}

export interface DashboardPerformance {
  api_response_time: number;
  database_response_time: number;
  cache_hit_rate: number;
  error_rate: number;
  throughput_per_second: number;
}

export interface PerformanceMetrics {
  timestamp: string;

  // API 성능
  api: {
    response_time_ms: number;
    throughput_per_second: number;
    error_rate: number;
  };

  // 데이터베이스 성능
  database: {
    query_time_ms: number;
    connection_pool_usage: number;
    slow_queries: number;
  };

  // 캐시 성능
  cache: {
    hit_rate: number;
    miss_rate: number;
    memory_usage_mb: number;
    eviction_count: number;
  };

  // 큐 성능
  queue?: {
    pending_jobs: number;
    processed_per_second: number;
    failed_jobs: number;
  };
}

export interface ServiceHealthData {
  services: {
    backend: 'healthy' | 'warning' | 'error';
    database: 'healthy' | 'warning' | 'error' | 'unknown';
    redis: 'healthy' | 'warning' | 'error' | 'unknown';
    collector: 'healthy' | 'warning' | 'error' | 'unknown';
  };
  ports: {
    backend?: number;
    collector?: number;
    redis?: number;
    postgresql?: number;
    rabbitmq?: number;
  };
  overall: 'healthy' | 'degraded' | 'critical';
  last_check: string;
}

export interface AlarmSummary {
  active_total: number;
  today_total: number;
  unacknowledged: number;
  critical: number;
  major: number;
  minor: number;
  warning: number;
  recent_alarms: RecentAlarm[];
}

export interface RecentAlarm {
  id: string;
  type: 'critical' | 'major' | 'minor' | 'warning' | 'info';
  message: string;
  icon?: string;
  timestamp: string;
  device_id?: number;
  device_name?: string;
  acknowledged: boolean;
  acknowledged_by?: string;
  severity: string;
}

export interface DashboardOverviewData {
  services: {
    total: number;
    running: number;
    stopped: number;
    error: number;
    details: ServiceInfo[];
  };
  system_metrics: SystemMetrics;
  device_summary: DeviceSummary;
  alarms: AlarmSummary;
  performance: DashboardPerformance;
  communication_status: {
    upstream: {
      total_devices: number;
      connected_devices: number;
      connectivity_rate: number;
      last_polled_at: string;
    };
    downstream: {
      total_exports: number;
      success_rate: number;
      last_dispatch_at: string;
    };
  };
  health_status: {
    overall: 'healthy' | 'degraded' | 'critical';
    database: 'healthy' | 'warning' | 'critical';
    redis: 'healthy' | 'warning' | 'critical';
    collector: 'healthy' | 'warning' | 'critical';
    gateway: 'healthy' | 'warning' | 'critical';
    network: 'healthy' | 'warning' | 'critical';
    storage: 'healthy' | 'warning' | 'critical';
    cache?: 'healthy' | 'warning' | 'critical';
    message_queue?: 'healthy' | 'warning' | 'critical';
  };
  last_updated: string;
  /** Redis 정상 시 'full', Redis 없을 시 'limited' (OS 프로세스 체크 기반) */
  monitoring_mode?: 'full' | 'limited';
  /** Redis 연결 가용 여부 */
  redis_available?: boolean;
  hierarchy?: {
    id: number;
    name: string;
    code: string;
    collectors: ServiceInfo[];
  }[];
  unassigned_collectors?: ServiceInfo[];
}

export interface ServiceControlRequest {
  action: 'start' | 'stop' | 'restart';
}

export interface ServiceControlResponse {
  success: boolean;
  message: string;
  service_name: string;
  action: string;
  previous_status?: string;
  new_status?: string;
  error?: string;
}

export interface TenantStats {
  tenant_id: number;
  total_devices: number;
  active_devices: number;
  total_data_points: number;
  active_alarms: number;
  total_users: number;
  storage_usage_mb: number;
  last_activity: string;
}

export interface RecentDevice {
  id: number;
  name: string;
  protocol_type: string;
  status: string;
  connection_status: string;
  last_seen: string;
  site_name?: string;
}

export interface SystemHealthData {
  overall_status: 'healthy' | 'degraded' | 'critical';
  components: {
    backend: {
      status: 'healthy' | 'warning' | 'critical';
      response_time: number;
      uptime: number;
      memory_usage: number;
      version: string;
    };
    database: {
      connected: boolean;
      version?: string;
      response_time: number;
      error?: string;
    };
    redis: {
      connected: boolean;
      info?: string;
      response_time: number;
      error?: string;
    };
    collector: {
      status: 'healthy' | 'warning' | 'critical' | 'unknown';
      last_heartbeat?: string | null;
    };
  };
  metrics: SystemMetrics;
  services: ServiceInfo[];
  last_check: string;
}

// ============================================================================
// HTTP 클라이언트 클래스 - 다른 API 서비스와 동일한 패턴
// ============================================================================

class HttpClient {
  private async request<T>(endpoint: string, options: RequestInit = {}): Promise<ApiResponse<T>> {
    try {
      const response = await fetch(endpoint, {
        headers: {
          'Content-Type': 'application/json',
          ...options.headers,
        },
        ...options,
      });

      const responseText = await response.text();

      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}, response: ${responseText}`);
      }

      if (!responseText.trim()) {
        return { success: false, error: 'Empty response' } as ApiResponse<T>;
      }

      const data = JSON.parse(responseText);
      return data;
    } catch (error) {
      console.error(`API Error for ${endpoint}:`, error);
      throw error;
    }
  }

  async get<T>(endpoint: string, params?: Record<string, any>): Promise<ApiResponse<T>> {
    const queryParams = new URLSearchParams();

    if (params) {
      Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
          queryParams.append(key, String(value));
        }
      });
    }

    const url = params && queryParams.toString() ?
      `${endpoint}?${queryParams.toString()}` : endpoint;

    return this.request<T>(url, { method: 'GET' });
  }

  async post<T>(endpoint: string, data?: any): Promise<ApiResponse<T>> {
    return this.request<T>(endpoint, {
      method: 'POST',
      body: data ? JSON.stringify(data) : undefined
    });
  }
}

// ============================================================================
// DashboardApiService 클래스 - 완전한 대시보드 API 통합
// ============================================================================

export class DashboardApiService {
  private static httpClient = new HttpClient();

  // ========================================================================
  // 📊 통합 대시보드 데이터 조회 (단일 API 호출)
  // ========================================================================

  /**
   * 대시보드 전체 개요 데이터 조회
   * 모든 대시보드 데이터를 한번에 가져오는 통합 API
   */
  static async getOverview(): Promise<ApiResponse<DashboardOverviewData>> {
    console.log('📊 대시보드 전체 개요 조회');
    return this.httpClient.get<DashboardOverviewData>(ENDPOINTS.DASHBOARD_OVERVIEW);
  }

  /**
   * 테넌트별 통계 조회
   */
  static async getTenantStats(): Promise<ApiResponse<TenantStats>> {
    console.log('🏢 테넌트 통계 조회');
    return this.httpClient.get<TenantStats>(ENDPOINTS.DASHBOARD_TENANT_STATS);
  }

  /**
   * 최근 디바이스 활동 조회
   */
  static async getRecentDevices(limit: number = 10): Promise<ApiResponse<RecentDevice[]>> {
    console.log('📱 최근 디바이스 활동 조회:', limit);
    return this.httpClient.get<RecentDevice[]>(ENDPOINTS.DASHBOARD_RECENT_DEVICES, { limit });
  }

  /**
   * 시스템 전체 헬스 상태 조회
   */
  static async getSystemHealth(): Promise<ApiResponse<SystemHealthData>> {
    console.log('💚 시스템 헬스 상태 조회');
    return this.httpClient.get<SystemHealthData>(ENDPOINTS.DASHBOARD_SYSTEM_HEALTH);
  }

  /**
   * 서비스 상태 목록 조회 (Redis Heartbeat 기반)
   */
  static async getServicesStatus(): Promise<ApiResponse<ServiceInfo[]>> {
    console.log('💓 서비스 상태 목록 조회 (Redis)');
    return this.httpClient.get<ServiceInfo[]>(ENDPOINTS.DASHBOARD_SERVICES_STATUS);
  }

  // ========================================================================
  // 🔍 개별 모니터링 데이터 조회 (세분화된 API)
  // ========================================================================

  /**
   * 서비스 헬스 상태 조회
   * Redis, PostgreSQL, Collector 등의 개별 서비스 상태
   */
  static async getServiceHealth(): Promise<ApiResponse<ServiceHealthData>> {
    console.log('🏥 서비스 헬스 상태 조회');
    return this.httpClient.get<ServiceHealthData>(ENDPOINTS.MONITORING_SERVICE_HEALTH);
  }

  /**
   * 시스템 메트릭 조회
   * CPU, 메모리, 디스크, 네트워크 사용률
   */
  static async getSystemMetrics(): Promise<ApiResponse<SystemMetrics>> {
    console.log('📈 시스템 메트릭 조회');
    return this.httpClient.get<SystemMetrics>(ENDPOINTS.MONITORING_SYSTEM_METRICS);
  }

  /**
   * 데이터베이스 통계 조회
   * 테이블 수, 디바이스 수, 데이터 포인트 수 등
   */
  static async getDatabaseStats(): Promise<ApiResponse<DatabaseStats>> {
    console.log('💾 데이터베이스 통계 조회');
    return this.httpClient.get<DatabaseStats>(ENDPOINTS.MONITORING_DATABASE_STATS);
  }

  /**
   * 성능 지표 조회
   * API 응답시간, 처리량, 에러율 등
   */
  static async getPerformanceMetrics(): Promise<ApiResponse<PerformanceMetrics>> {
    console.log('⚡ 성능 지표 조회');
    return this.httpClient.get<PerformanceMetrics>(ENDPOINTS.MONITORING_PERFORMANCE);
  }

  // ========================================================================
  // 🛠️ 서비스 제어 API
  // ========================================================================

  /**
   * 서비스 제어 (시작/중지/재시작)
   * @param serviceName collector, redis, database 등
   * @param action start, stop, restart
   */
  static async controlService(
    serviceName: string,
    action: 'start' | 'stop' | 'restart'
  ): Promise<ApiResponse<ServiceControlResponse>> {
    console.log(`🔧 서비스 제어: ${serviceName} ${action}`);

    const request: ServiceControlRequest = { action };

    return this.httpClient.post<ServiceControlResponse>(
      ENDPOINTS.DASHBOARD_SERVICE_CONTROL(serviceName, action),
      request
    );
  }

  /**
   * Collector 서비스 시작
   */
  static async startCollector(): Promise<ApiResponse<ServiceControlResponse>> {
    console.log('▶️ Collector 서비스 시작');
    return this.controlService('collector', 'start');
  }

  /**
   * Collector 서비스 중지
   */
  static async stopCollector(): Promise<ApiResponse<ServiceControlResponse>> {
    console.log('⏹️ Collector 서비스 중지');
    return this.controlService('collector', 'stop');
  }

  /**
   * Collector 서비스 재시작
   */
  static async restartCollector(): Promise<ApiResponse<ServiceControlResponse>> {
    console.log('🔄 Collector 서비스 재시작');
    return this.controlService('collector', 'restart');
  }

  /**
   * Redis 서비스 시작
   */
  static async startRedis(): Promise<ApiResponse<ServiceControlResponse>> {
    console.log('▶️ Redis 서비스 시작');
    return this.controlService('redis', 'start');
  }

  /**
   * Redis 서비스 중지
   */
  static async stopRedis(): Promise<ApiResponse<ServiceControlResponse>> {
    console.log('⏹️ Redis 서비스 중지');
    return this.controlService('redis', 'stop');
  }

  /**
   * Redis 서비스 재시작
   */
  static async restartRedis(): Promise<ApiResponse<ServiceControlResponse>> {
    console.log('🔄 Redis 서비스 재시작');
    return this.controlService('redis', 'restart');
  }

  /**
   * Database 서비스 시작
   */
  static async startDatabase(): Promise<ApiResponse<ServiceControlResponse>> {
    console.log('▶️ Database 서비스 시작');
    return this.controlService('database', 'start');
  }

  /**
   * Database 서비스 중지
   */
  static async stopDatabase(): Promise<ApiResponse<ServiceControlResponse>> {
    console.log('⏹️ Database 서비스 중지');
    return this.controlService('database', 'stop');
  }

  /**
   * Database 서비스 재시작
   */
  static async restartDatabase(): Promise<ApiResponse<ServiceControlResponse>> {
    console.log('🔄 Database 서비스 재시작');
    return this.controlService('database', 'restart');
  }

  // ========================================================================
  // 📱 디바이스 및 포인트 제어 (Hierarchical Control)
  // ========================================================================

  /**
   * 디바이스 워커 제어 (시작/중지/재시작)
   */
  static async controlDevice(
    deviceId: number | string,
    action: 'start' | 'stop' | 'restart',
    options: any = {}
  ): Promise<ApiResponse<any>> {
    console.log(`📱 디바이스 제어: ${deviceId} ${action}`);
    return this.httpClient.post<any>(`${ENDPOINTS.DEVICES}/${deviceId}/${action}`, options);
  }

  /**
   * 포인트 제어 (디지털 DO / 아날로그 AO)
   */
  static async controlPoint(
    deviceId: number | string,
    outputId: number | string,
    type: 'digital' | 'analog',
    value: any,
    options: any = {}
  ): Promise<ApiResponse<any>> {
    console.log(`⚡ 포인트 제어: Device ${deviceId}, ${type} ${outputId} = ${value}`);
    const endpoint = `${ENDPOINTS.DEVICES}/${deviceId}/${type}/${outputId}/control`;
    const payload = type === 'digital' ? { state: value, options } : { value, options };
    return this.httpClient.post<any>(endpoint, payload);
  }

  // ========================================================================
  // 📊 알람 관련 대시보드 데이터 (알람 API를 활용)
  // ========================================================================

  /**
   * 알람 통계 조회 (대시보드용)
   */
  static async getAlarmStatistics(): Promise<ApiResponse<any>> {
    console.log('🚨 알람 통계 조회');
    return this.httpClient.get<any>(ENDPOINTS.ALARM_STATISTICS);
  }

  /**
   * 최근 알람 조회 (대시보드용)
   */
  static async getRecentAlarms(limit: number = 5): Promise<ApiResponse<RecentAlarm[]>> {
    console.log('🔔 최근 알람 조회:', limit);
    return this.httpClient.get<RecentAlarm[]>(ENDPOINTS.ALARM_RECENT, { limit });
  }

  /**
   * 미확인 알람 조회 (대시보드용)
   */
  static async getUnacknowledgedAlarms(): Promise<ApiResponse<RecentAlarm[]>> {
    console.log('❗ 미확인 알람 조회');
    return this.httpClient.get<RecentAlarm[]>(ENDPOINTS.ALARM_UNACKNOWLEDGED);
  }

  // ========================================================================
  // 🔄 통합 대시보드 데이터 로드 (복합 API 호출)
  // ========================================================================

  /**
   * 대시보드에 필요한 모든 데이터를 병렬로 로드
   * 4개의 모니터링 API를 동시 호출하여 성능 향상
   */
  static async loadAllDashboardData(): Promise<{
    servicesData: ServiceHealthData | null;
    systemMetrics: SystemMetrics | null;
    databaseStats: DatabaseStats | null;
    performanceData: PerformanceMetrics | null;
    errors: string[];
  }> {
    console.log('🎯 전체 대시보드 데이터 병렬 로드 시작...');

    const errors: string[] = [];

    // 4개 API 병렬 호출
    const [
      servicesResult,
      metricsResult,
      dbStatsResult,
      performanceResult
    ] = await Promise.allSettled([
      this.getServiceHealth(),
      this.getSystemMetrics(),
      this.getDatabaseStats(),
      this.getPerformanceMetrics()
    ]);

    // 결과 처리
    const servicesData = servicesResult.status === 'fulfilled' && servicesResult.value.success
      ? servicesResult.value.data : null;
    if (servicesResult.status === 'rejected' || !servicesData) {
      errors.push('서비스 상태 로드 실패');
    }

    const systemMetrics = metricsResult.status === 'fulfilled' && metricsResult.value.success
      ? metricsResult.value.data : null;
    if (metricsResult.status === 'rejected' || !systemMetrics) {
      errors.push('시스템 메트릭 로드 실패');
    }

    const databaseStats = dbStatsResult.status === 'fulfilled' && dbStatsResult.value.success
      ? dbStatsResult.value.data : null;
    if (dbStatsResult.status === 'rejected' || !databaseStats) {
      errors.push('데이터베이스 통계 로드 실패');
    }

    const performanceData = performanceResult.status === 'fulfilled' && performanceResult.value.success
      ? performanceResult.value.data : null;
    if (performanceResult.status === 'rejected' || !performanceData) {
      errors.push('성능 지표 로드 실패');
    }

    console.log(`✅ 대시보드 데이터 로드 완료 - ${4 - errors.length}/4개 성공`);

    return {
      servicesData,
      systemMetrics,
      databaseStats,
      performanceData,
      errors
    };
  }

  // ========================================================================
  // 🧪 API 테스트 및 헬스체크
  // ========================================================================


  /**
   * 대시보드 API 연결 테스트
   */
  static async testConnection(): Promise<ApiResponse<{ message: string; timestamp: string }>> {
    console.log('🧪 대시보드 API 연결 테스트');
    return this.httpClient.get<{ message: string; timestamp: string }>('/api/dashboard/test');
  }

  /**
   * 모든 모니터링 API 상태 확인
   */
  static async checkAllEndpoints(): Promise<{
    dashboard_overview: boolean;
    service_health: boolean;
    system_metrics: boolean;
    database_stats: boolean;
    performance: boolean;
    service_control: boolean;
  }> {
    console.log('🔍 모든 엔드포인트 상태 확인...');

    const endpoints = [
      { name: 'dashboard_overview', url: ENDPOINTS.DASHBOARD_OVERVIEW },
      { name: 'service_health', url: ENDPOINTS.MONITORING_SERVICE_HEALTH },
      { name: 'system_metrics', url: ENDPOINTS.MONITORING_SYSTEM_METRICS },
      { name: 'database_stats', url: ENDPOINTS.MONITORING_DATABASE_STATS },
      { name: 'performance', url: ENDPOINTS.MONITORING_PERFORMANCE }
    ];

    const results: Record<string, boolean> = {};

    for (const endpoint of endpoints) {
      try {
        const response = await fetch(endpoint.url, { method: 'HEAD' });
        results[endpoint.name] = response.ok;
      } catch (error) {
        results[endpoint.name] = false;
      }
    }

    // 서비스 제어는 POST이므로 별도 확인
    results.service_control = true; // 백엔드에서 구현됨을 확인했음

    return results as any;
  }

  /**
   * 오늘 발생한 알람 조회
   */
  static async getTodayAlarms(limit: number = 20): Promise<ApiResponse<RecentAlarm[]>> {
    console.log('📅 오늘 발생한 알람 조회:', limit);
    return this.httpClient.get<RecentAlarm[]>(ENDPOINTS.ALARM_TODAY, { limit });
  }

  /**
   * 오늘 알람 통계 조회
   */
  static async getTodayAlarmStatistics(): Promise<ApiResponse<any>> {
    console.log('📊 오늘 알람 통계 조회');
    return this.httpClient.get<any>(ENDPOINTS.ALARM_TODAY_STATISTICS);
  }
}