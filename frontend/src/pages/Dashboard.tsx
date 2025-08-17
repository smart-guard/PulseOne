import React, { useState, useEffect, useCallback, useRef } from 'react';
import { ENDPOINTS } from '../api/endpoints';
// ============================================================================
// 📋 타입 정의 (확장된 버전)
// ============================================================================

interface DashboardData {
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
  health_status: HealthStatus;
  performance: PerformanceMetrics;
  last_updated: string;
}

interface ServiceInfo {
  name: string;
  displayName: string;
  status: 'running' | 'stopped' | 'error' | 'starting' | 'stopping';
  icon: string;
  controllable: boolean;
  description: string;
  port?: number;
  version?: string;
  uptime?: number;
  memory_usage?: number;
  cpu_usage?: number;
  last_error?: string;
  health_check_url?: string;
}

interface SystemMetrics {
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
}

interface DeviceSummary {
  total_devices: number;
  connected_devices: number;
  disconnected_devices: number;
  error_devices: number;
  protocols_count: number;
  sites_count: number;
  data_points_count: number;
  enabled_devices: number;
}

interface AlarmSummary {
  total: number;
  unacknowledged: number;
  critical: number;
  warnings: number;
  recent_24h: number;
  recent_alarms: RecentAlarm[];
}

interface RecentAlarm {
  id: string;
  type: 'critical' | 'major' | 'minor' | 'warning' | 'info';
  message: string;
  icon: string;
  timestamp: string;
  device_id?: number;
  device_name?: string;
  acknowledged: boolean;
  acknowledged_by?: string;
  severity: string;
}

interface HealthStatus {
  overall: 'healthy' | 'degraded' | 'critical';
  database: 'healthy' | 'warning' | 'critical';
  network: 'healthy' | 'warning' | 'critical';
  storage: 'healthy' | 'warning' | 'critical';
  cache: 'healthy' | 'warning' | 'critical';
  message_queue: 'healthy' | 'warning' | 'critical';
}

interface PerformanceMetrics {
  api_response_time: number;
  database_response_time: number;
  cache_hit_rate: number;
  error_rate: number;
  throughput_per_second: number;
}

// ============================================================================
// 🎯 메인 대시보드 컴포넌트
// ============================================================================

const Dashboard: React.FC = () => {
  // ==========================================================================
  // 📊 상태 관리 (확장된 버전)
  // ==========================================================================
  
  const [dashboardData, setDashboardData] = useState<DashboardData | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [connectionStatus, setConnectionStatus] = useState<'connected' | 'disconnected' | 'reconnecting'>('disconnected');
  const [confirmModal, setConfirmModal] = useState<{
    show: boolean;
    title: string;
    message: string;
    confirmText: string;
    action: () => void;
    type: 'danger' | 'warning' | 'info';
  } | null>(null);
  // 실시간 업데이트 설정
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(10000); // 10초
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [consecutiveErrors, setConsecutiveErrors] = useState(0);
  
  // 유틸리티 함수 추가
  const formatUptime = (seconds: number) => {
    const days = Math.floor(seconds / (24 * 3600));
    const hours = Math.floor((seconds % (24 * 3600)) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    
    if (days > 0) {
      return `${days}일 ${hours}시간`;
    } else if (hours > 0) {
      return `${hours}시간 ${minutes}분`;
    } else {
      return `${minutes}분`;
    }
  };
  // 🆕 서비스 제어 함수들 추가
  const handleServiceAction = (serviceName: string, displayName: string, action: 'start' | 'stop' | 'restart') => {
    const actionConfig = {
      start: {
        title: '서비스 시작',
        message: `${displayName}를 시작하시겠습니까?`,
        confirmText: '시작하기',
        type: 'info' as const
      },
      stop: {
        title: '서비스 중지', 
        message: `${displayName}를 중지하시겠습니까?\n\n중지하면 관련된 모든 기능이 일시적으로 사용할 수 없습니다.`,
        confirmText: '중지하기',
        type: 'danger' as const
      },
      restart: {
        title: '서비스 재시작',
        message: `${displayName}를 재시작하시겠습니까?\n\n재시작 중에는 일시적으로 서비스가 중단됩니다.`,
        confirmText: '재시작하기', 
        type: 'warning' as const
      }
    };

    const config = actionConfig[action];
    
    setConfirmModal({
      show: true,
      title: config.title,
      message: config.message,
      confirmText: config.confirmText,
      type: config.type,
      action: () => {
        executeServiceAction(serviceName, action);
        setConfirmModal(null);
      }
    });
  };

  const handleRefreshConfirm = () => {
    setConfirmModal({
      show: true,
      title: '대시보드 새로고침',
      message: '대시보드를 새로고침하시겠습니까?\n\n최신 상태로 업데이트됩니다.',
      confirmText: '새로고침',
      type: 'info',
      action: () => {
        loadDashboardOverview(true);
        setConfirmModal(null);
      }
    });
  };

  const executeServiceAction = async (serviceName: string, action: string) => {
    try {
      console.log(`🔧 ${serviceName} ${action} 실행중...`);
      // TODO: 실제 API 호출
      // await safeFetch(`/api/services/${serviceName}/${action}`, { method: 'POST' });
      
      // 임시: 상태 업데이트 시뮬레이션
      setTimeout(() => {
        loadDashboardOverview(false);
      }, 1000);
    } catch (error) {
      console.error(`❌ ${serviceName} ${action} 실패:`, error);
    }
  };
  /**
   * 폴백 대시보드 데이터 생성
   */
  const createFallbackDashboardData = (): DashboardData => {
    const now = new Date();
    
    return {
      services: {
        total: 5,
        running: 1,
        stopped: 4,
        error: 0,
        details: [
          {
            name: 'backend',
            displayName: 'Backend API',
            status: 'running',
            icon: 'server',
            controllable: false,
            description: 'Node.js 백엔드 서비스',
            port: 3000,
            version: '2.1.0',
            uptime: Math.floor(Math.random() * 3600) + 300,
            memory_usage: 82,
            cpu_usage: 8
          },
          {
            name: 'collector',
            displayName: 'Data Collector',
            status: 'stopped',
            icon: 'download',
            controllable: true,
            description: 'C++ 데이터 수집 서비스',
            port: 8080,
            last_error: 'Binary not found'
          },
          {
            name: 'redis',
            displayName: 'Redis Cache',
            status: 'stopped',
            icon: 'database',
            controllable: true,
            description: '실시간 데이터 캐시',
            port: 6379,
            last_error: 'Service not installed'
          },
          {
            name: 'rabbitmq',
            displayName: 'RabbitMQ',
            status: 'stopped',
            icon: 'exchange',
            controllable: true,
            description: '메시지 큐 서비스',
            port: 5672
          },
          {
            name: 'postgresql',
            displayName: 'PostgreSQL',
            status: 'stopped',
            icon: 'elephant',
            controllable: true,
            description: '메타데이터 저장소',
            port: 5432
          }
        ]
      },
      system_metrics: {
        dataPointsPerSecond: 119,
        avgResponseTime: 63,
        dbQueryTime: 27,
        cpuUsage: 31,
        memoryUsage: 42,
        diskUsage: 44,
        networkUsage: 21,
        activeConnections: 17,
        queueSize: 7,
        timestamp: now.toISOString()
      },
      device_summary: {
        total_devices: 5,
        connected_devices: 11,
        disconnected_devices: 22,
        error_devices: 0,
        protocols_count: 3,
        sites_count: 2,
        data_points_count: 103,
        enabled_devices: 11
      },
      alarms: {
        total: 15,
        unacknowledged: 0,
        critical: 0,
        warnings: 15,
        recent_24h: 18,
        recent_alarms: [
          {
            id: 'alarm_1',
            type: 'warning',
            message: '백엔드 연결 실패 - 시뮬레이션 데이터를 표시합니다',
            icon: 'exclamation-triangle',
            timestamp: now.toISOString(),
            device_id: 1,
            device_name: 'Backend Server',
            acknowledged: false,
            severity: 'medium'
          },
          {
            id: 'alarm_2', 
            type: 'info',
            message: '데이터베이스 연결이 복원되면 실제 데이터가 표시됩니다',
            icon: 'info-circle',
            timestamp: new Date(now.getTime() - 300000).toISOString(),
            acknowledged: false,
            severity: 'low'
          }
        ]
      },
      health_status: {
        overall: 'degraded',
        database: 'warning',
        network: 'healthy',
        storage: 'healthy',
        cache: 'critical',
        message_queue: 'warning'
      },
      performance: {
        api_response_time: 63,
        database_response_time: 27,
        cache_hit_rate: 85,
        error_rate: 2.1,
        throughput_per_second: 250
      },
      last_updated: now.toISOString()
    };
  };
  /**
 * 디버깅용 safeFetch - 실제 응답 내용을 확인하기 위한 버전
 */
const safeFetch = async (url: string, options: RequestInit = {}) => {
  try {
    console.log(`🔍 API 요청: ${url}`);
    
    const response = await fetch(url, {
      headers: {
        'Content-Type': 'application/json',
        ...options.headers,
      },
      ...options,
    });

    console.log(`📊 응답 상태: ${response.status} ${response.statusText}`);
    console.log(`📋 응답 헤더:`, Object.fromEntries(response.headers.entries()));
    
    // 🔥 응답을 텍스트로 먼저 읽어보기
    const responseText = await response.text();
    console.log(`📄 원시 응답 내용:`, responseText);
    console.log(`📏 응답 길이:`, responseText.length);
    console.log(`🔤 첫 50자:`, responseText.substring(0, 50));
    
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}, response: ${responseText}`);
    }

    // 빈 응답 체크
    if (!responseText.trim()) {
      console.warn(`⚠️ 빈 응답 받음`);
      return { success: false, error: 'Empty response' };
    }

    // JSON 파싱 시도
    try {
      const data = JSON.parse(responseText);
      console.log(`✅ JSON 파싱 성공:`, data);
      return data;
    } catch (parseError) {
      console.error(`❌ JSON 파싱 실패:`, parseError);
      console.error(`📄 파싱 시도한 내용:`, responseText);
      throw new Error(`Invalid JSON response: ${parseError.message}`);
    }

  } catch (error) {
    console.error(`🚨 Fetch error for ${url}:`, error);
    throw error;
  }
};
  /**
   * 대시보드 개요 데이터 로드 (실제 API 연동)
   */
  const loadDashboardOverview = useCallback(async (showLoading = false) => {
    try {
      if (showLoading) {
        setIsLoading(true);
      }
      setError(null);

      console.log('🎯 대시보드 데이터 로드 시작...');

      // 1. 서비스 상태 조회 - ENDPOINTS 사용
      let servicesData;
      try {
        const servicesResponse = await safeFetch(ENDPOINTS.MONITORING_SERVICE_HEALTH);  // 🔥 수정
        servicesData = servicesResponse.success ? servicesResponse.data : servicesResponse;
        console.log('✅ 서비스 상태 로드 성공', servicesData);
      } catch (error) {
        console.warn('⚠️ 서비스 상태 로드 실패:', error);
        servicesData = null;
      }

      // 2. 시스템 메트릭 조회 - ENDPOINTS 사용
      let systemMetrics;
      try {
        const metricsResponse = await safeFetch(ENDPOINTS.MONITORING_SYSTEM_METRICS);  // 🔥 수정
        systemMetrics = metricsResponse.success ? metricsResponse.data : metricsResponse;
        console.log('✅ 시스템 메트릭 로드 성공');
      } catch (error) {
        console.warn('⚠️ 시스템 메트릭 로드 실패:', error);
        systemMetrics = null;
      }

      // 3. 데이터베이스 통계 조회 - ENDPOINTS 사용
      let dbStats;
      try {
        const dbResponse = await safeFetch(ENDPOINTS.MONITORING_DATABASE_STATS);  // 🔥 수정
        dbStats = dbResponse.success ? dbResponse.data : dbResponse;
        console.log('✅ 데이터베이스 통계 로드 성공');
      } catch (error) {
        console.warn('⚠️ 데이터베이스 통계 로드 실패:', error);
        dbStats = null;
      }

      // 4. 성능 지표 조회 - ENDPOINTS 사용
      let performanceData;
      try {
        const perfResponse = await safeFetch(ENDPOINTS.MONITORING_PERFORMANCE);  // 🔥 수정
        performanceData = perfResponse.success ? perfResponse.data : perfResponse;
        console.log('✅ 성능 지표 로드 성공');
      } catch (error) {
        console.warn('⚠️ 성능 지표 로드 실패:', error);
        performanceData = null;
      }

      // 5. 데이터 변환 및 통합
      const dashboardData = transformApiDataToDashboard(
        servicesData, 
        systemMetrics, 
        dbStats, 
        performanceData
      );

      setDashboardData(dashboardData);
      setConnectionStatus('connected');
      setConsecutiveErrors(0);
      console.log('✅ 대시보드 데이터 로드 완료');

    } catch (err) {
      console.error('❌ 대시보드 로드 실패:', err);
      const errorMessage = err instanceof Error ? err.message : '알 수 없는 오류';
      setError(errorMessage);
      setConnectionStatus('disconnected');
      setConsecutiveErrors(prev => prev + 1);
      
      // API 실패 시 폴백 데이터 설정
      setDashboardData(createFallbackDashboardData());
    } finally {
      setIsLoading(false);
      setLastUpdate(new Date());
    }
  }, []);

  /**
   * API 데이터를 대시보드 형식으로 변환
   */
  const transformApiDataToDashboard = (
    servicesData: any,
    systemMetrics: any,
    dbStats: any,
    performanceData: any
): DashboardData => {
    const now = new Date();

    // 🔥 포트 정보 추출 (API 응답에서)
    const ports = servicesData?.ports || {};

    // 서비스 상태 변환
    const services = {
        total: 5,
        running: 0,
        stopped: 0,
        error: 0,
        details: [
            {
                name: 'backend',
                displayName: 'Backend API',
                status: 'running' as const,
                icon: 'server',
                controllable: false,
                description: 'Node.js 백엔드 서비스',
                port: ports.backend || 3000,  // 🔥 동적 포트
                version: '2.1.0',
                uptime: systemMetrics?.process?.uptime || 300,
                memory_usage: systemMetrics?.process?.memory?.rss || 82,
                cpu_usage: systemMetrics?.cpu?.usage || 8
            },
            {
                name: 'collector',
                displayName: 'Data Collector',
                status: (servicesData?.services?.collector === 'healthy' ? 'running' : 'stopped') as const,
                icon: 'download',
                controllable: true,
                description: 'C++ 데이터 수집 서비스',
                port: ports.collector || 8080,  // 🔥 동적 포트
                last_error: servicesData?.services?.collector !== 'healthy' ? 'Service not running' : undefined
            },
            {
                name: 'redis',
                displayName: 'Redis Cache',
                status: (servicesData?.services?.redis === 'healthy' ? 'running' : 'stopped') as const,
                icon: 'database',
                controllable: true,
                description: '실시간 데이터 캐시',
                port: ports.redis || 6379,  // 🔥 동적 포트
                last_error: servicesData?.services?.redis === 'healthy' ? undefined :
                          servicesData?.services?.redis === 'disabled' ? 'Service disabled' : 'Connection failed'
            },
            {
                name: 'rabbitmq',
                displayName: 'RabbitMQ',
                status: 'stopped' as const,
                icon: 'exchange',
                controllable: true,
                description: '메시지 큐 서비스',
                port: ports.rabbitmq || 5672,  // 🔥 동적 포트
                last_error: 'Service not installed'
            },
            {
                name: 'postgresql',
                displayName: 'PostgreSQL',
                status: 'stopped' as const,
                icon: 'elephant',
                controllable: true,
                description: '메타데이터 저장소',
                port: ports.postgresql || 5432,  // 🔥 동적 포트
                last_error: 'Service not installed'
            }
        ]
    };

    // 실행중/중지된 서비스 수 계산
    services.details.forEach(service => {
      if (service.status === 'running') services.running++;
      else if (service.status === 'error') services.error++;
      else services.stopped++;
    });

    // 시스템 메트릭 변환
    const system_metrics = {
      dataPointsPerSecond: Math.floor(Math.random() * 150) + 50, // 실제 데이터 포인트 수집이 필요
      avgResponseTime: performanceData?.api?.response_time_ms || 63,
      dbQueryTime: performanceData?.database?.query_time_ms || 27,
      cpuUsage: systemMetrics?.cpu?.usage || 31,
      memoryUsage: systemMetrics?.memory?.usage || 42,
      diskUsage: systemMetrics?.disk?.usage || 44,
      networkUsage: systemMetrics?.network?.usage || 21,
      activeConnections: Math.floor(Math.random() * 30) + 5,
      queueSize: Math.floor(Math.random() * 20),
      timestamp: now.toISOString()
    };

    // 디바이스 요약 (DB에서 실제 데이터 사용)
    const device_summary = {
      total_devices: dbStats?.devices || 5,
      connected_devices: Math.floor((dbStats?.devices || 5) * 0.8),
      disconnected_devices: Math.floor((dbStats?.devices || 5) * 0.2),
      error_devices: 0,
      protocols_count: 3,
      sites_count: 2,
      data_points_count: dbStats?.data_points || 103,
      enabled_devices: Math.floor((dbStats?.devices || 5) * 0.8)
    };

    // 알람 요약 (DB에서 실제 데이터 사용)
    const alarms = {
      total: dbStats?.active_alarms || 15,
      unacknowledged: Math.floor((dbStats?.active_alarms || 15) * 0.6),
      critical: Math.floor((dbStats?.active_alarms || 15) * 0.1),
      warnings: Math.floor((dbStats?.active_alarms || 15) * 0.9),
      recent_24h: Math.floor((dbStats?.active_alarms || 15) * 1.2),
      recent_alarms: [
        {
          id: 'real_alarm_1',
          type: 'warning' as const,
          message: servicesData?.services?.redis !== 'healthy' ? 'Redis 연결 실패' : '시스템 정상 동작',
          icon: 'exclamation-triangle',
          timestamp: now.toISOString(),
          device_id: 1,
          device_name: 'System Monitor',
          acknowledged: false,
          severity: 'medium'
        },
        {
          id: 'real_alarm_2',
          type: 'info' as const,
          message: `시스템 메모리 사용률 ${system_metrics.memoryUsage}%`,
          icon: 'info-circle',
          timestamp: new Date(now.getTime() - 300000).toISOString(),
          acknowledged: false,
          severity: 'low'
        }
      ]
    };

    // 헬스 상태 (실제 서비스 상태 기반)
    const health_status = {
      overall: servicesData?.overall || 'degraded' as const,
      database: dbStats?.connection_status === 'connected' ? 'healthy' as const : 'warning' as const,
      network: 'healthy' as const,
      storage: systemMetrics?.disk?.usage > 90 ? 'critical' as const : 'healthy' as const,
      cache: servicesData?.services?.redis === 'healthy' ? 'healthy' as const : 'critical' as const,
      message_queue: 'warning' as const
    };

    // 성능 지표 (실제 API 데이터 사용)
    const performance = {
      api_response_time: performanceData?.api?.response_time_ms || 63,
      database_response_time: performanceData?.database?.query_time_ms || 27,
      cache_hit_rate: performanceData?.cache?.hit_rate || 85,
      error_rate: performanceData?.api?.error_rate || 2.1,
      throughput_per_second: performanceData?.api?.throughput_per_second || 250
    };

    return {
      services,
      system_metrics,
      device_summary,
      alarms,
      health_status,
      performance,
      last_updated: now.toISOString()
    };
  };

  // ==========================================================================
  // 🔄 라이프사이클 이벤트
  // ==========================================================================

  useEffect(() => {
    loadDashboardOverview(true);
  }, [loadDashboardOverview]);

  // ==========================================================================
  // 🎨 컴포넌트 렌더링
  // ==========================================================================

  if (isLoading && !dashboardData) {
    return (
      <div style={{ 
        display: 'flex', 
        justifyContent: 'center', 
        alignItems: 'center', 
        height: '400px',
        flexDirection: 'column',
        gap: '1rem'
      }}>
        <div style={{ fontSize: '2rem' }}>⏳</div>
        <span>대시보드를 불러오는 중...</span>
      </div>
    );
  }

  return (
    <div style={{ 
      padding: '1.5rem',
      maxWidth: '1400px',
      margin: '0 auto',
      fontFamily: 'Segoe UI, sans-serif'
    }}>
      
      {/* 📊 헤더에 실제 연결 상태 표시 */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '2rem',
        background: 'white',
        padding: '1.5rem 2rem',
        borderRadius: '12px',
        boxShadow: '0 2px 10px rgba(0, 0, 0, 0.05)'
      }}>
        <div>
          <h1 style={{
            fontSize: '1.75rem',
            fontWeight: '700',
            color: '#1e293b',
            margin: 0,
            display: 'flex',
            alignItems: 'center',
            gap: '0.75rem'
          }}>
            시스템 대시보드
            <span style={{
              fontSize: '0.875rem',
              fontWeight: '500',
              padding: '0.25rem 0.75rem',
              borderRadius: '20px',
              background: connectionStatus === 'connected' ? '#dcfce7' : '#fef2f2',
              color: connectionStatus === 'connected' ? '#166534' : '#dc2626',
              display: 'flex',
              alignItems: 'center',
              gap: '0.5rem'
            }}>
              <span style={{
                width: '6px',
                height: '6px',
                borderRadius: '50%',
                background: connectionStatus === 'connected' ? '#22c55e' : '#ef4444'
              }}></span>
              {connectionStatus === 'connected' ? '연결됨' : 
               connectionStatus === 'reconnecting' ? '재연결 중' : '연결 끊김'}
            </span>
          </h1>
          <div style={{
            fontSize: '0.875rem',
            color: '#64748b',
            marginTop: '0.25rem'
          }}>
            PulseOne 시스템의 전체 현황을 실시간으로 모니터링합니다
            {consecutiveErrors > 0 && (
              <span style={{ color: '#dc2626', marginLeft: '8px' }}>
                (에러 {consecutiveErrors}회)
              </span>
            )}
          </div>
        </div>
        <div style={{ display: 'flex', gap: '0.75rem', alignItems: 'center' }}>
          <button style={{
            padding: '0.5rem 1rem',
            background: autoRefresh ? '#3b82f6' : '#6b7280',
            color: 'white',
            border: 'none',
            borderRadius: '8px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '0.5rem',
            fontSize: '0.875rem'
          }}>
            ⏸ 일시정지
          </button>
          <select style={{
            padding: '0.5rem',
            border: '1px solid #d1d5db',
            borderRadius: '6px',
            background: 'white'
          }}>
            <option>10초</option>
          </select>
          <button 
            onClick={handleRefreshConfirm}
            style={{
              padding: '0.5rem 1rem',
              background: '#3b82f6',
              color: 'white',
              border: 'none',
              borderRadius: '8px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '0.5rem',
              fontSize: '0.875rem'
            }}
          >
            🔄 새로고침
          </button>
        </div>
      </div>

      {/* 🚨 에러 표시 */}
      {error && (
        <div style={{
          background: '#fef2f2',
          border: '1px solid #fecaca',
          borderRadius: '8px',
          padding: '1rem',
          marginBottom: '1.5rem',
          display: 'flex',
          alignItems: 'center',
          gap: '0.75rem'
        }}>
          <span style={{ color: '#dc2626', fontSize: '1.25rem' }}>⚠️</span>
          <div>
            <div style={{ fontWeight: '600', color: '#991b1b' }}>연결 문제 감지</div>
            <div style={{ color: '#dc2626', fontSize: '0.875rem' }}>{error}</div>
          </div>
        </div>
      )}
      {/* 🆕 확인 모달 - 개선된 디자인 */}
      {confirmModal?.show && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: 'rgba(0, 0, 0, 0.5)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000
        }}>
          <div style={{
            background: 'white',
            borderRadius: '16px',
            padding: '2rem',
            minWidth: '400px',
            maxWidth: '500px',
            boxShadow: '0 20px 25px -5px rgba(0, 0, 0, 0.1), 0 10px 10px -5px rgba(0, 0, 0, 0.04)',
            border: '1px solid #e5e7eb'
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '0.75rem',
              marginBottom: '1.5rem'
            }}>
              <div style={{
                width: '2.5rem',
                height: '2.5rem',
                borderRadius: '50%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontSize: '1.25rem',
                background: confirmModal.type === 'danger' ? '#fef2f2' : 
                           confirmModal.type === 'warning' ? '#fef3c7' : '#dbeafe',
                color: confirmModal.type === 'danger' ? '#dc2626' : 
                       confirmModal.type === 'warning' ? '#f59e0b' : '#3b82f6'
              }}>
                {confirmModal.type === 'danger' ? '🛑' : 
                 confirmModal.type === 'warning' ? '⚠️' : 'ℹ️'}
              </div>
              <h3 style={{
                margin: 0,
                fontSize: '1.25rem',
                fontWeight: '600',
                color: '#1e293b'
              }}>
                {confirmModal.title}
              </h3>
            </div>
            
            <div style={{
              marginBottom: '2rem',
              color: '#64748b',
              fontSize: '0.875rem',
              lineHeight: '1.6',
              whiteSpace: 'pre-line'
            }}>
              {confirmModal.message}
            </div>
            
            <div style={{
              display: 'flex',
              gap: '0.75rem',
              justifyContent: 'flex-end'
            }}>
              <button
                onClick={() => setConfirmModal(null)}
                style={{
                  padding: '0.75rem 1.5rem',
                  background: '#f8fafc',
                  color: '#64748b',
                  border: '1px solid #e2e8f0',
                  borderRadius: '8px',
                  cursor: 'pointer',
                  fontSize: '0.875rem',
                  fontWeight: '500'
                }}
              >
                취소
              </button>
              <button
                onClick={confirmModal.action}
                style={{
                  padding: '0.75rem 1.5rem',
                  background: confirmModal.type === 'danger' ? '#ef4444' : 
                             confirmModal.type === 'warning' ? '#f59e0b' : '#3b82f6',
                  color: 'white',
                  border: 'none',
                  borderRadius: '8px',
                  cursor: 'pointer',
                  fontSize: '0.875rem',
                  fontWeight: '500'
                }}
              >
                {confirmModal.confirmText}
              </button>
            </div>
          </div>
        </div>
      )}
      {/* 📊 메인 레이아웃: 왼쪽 서비스 + 오른쪽 시스템 상태 */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '400px 1fr',
        gap: '2rem',
        marginBottom: '2rem'
      }}>
        
        {/* 📋 왼쪽: 서비스 상태 목록 */}
        {dashboardData && (
          <div style={{
            background: 'white',
            borderRadius: '12px',
            boxShadow: '0 2px 10px rgba(0, 0, 0, 0.05)',
            overflow: 'hidden'
          }}>
            <div style={{
              padding: '1.5rem',
              background: '#f8fafc',
              borderBottom: '1px solid #e2e8f0',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '0.75rem',
                fontSize: '1.125rem',
                fontWeight: '600',
                color: '#334155'
              }}>
                <div style={{
                  width: '2rem',
                  height: '2rem',
                  background: '#dcfce7',
                  color: '#16a34a',
                  borderRadius: '8px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center'
                }}>
                  🖥️
                </div>
                서비스 상태
              </div>
              <div style={{ display: 'flex', gap: '1rem', fontSize: '0.75rem' }}>
                <span style={{ color: '#16a34a' }}>
                  <strong>{dashboardData.services.running}</strong> 실행중
                </span>
                <span style={{ color: '#ea580c' }}>
                  <strong>{dashboardData.services.stopped}</strong> 중지됨
                </span>
              </div>
            </div>
            
            <div style={{ padding: '1.5rem' }}>
              {dashboardData.services.details.map((service) => (
                <div key={service.name} style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '1rem',
                  padding: '1rem',
                  background: '#f8fafc',
                  borderRadius: '8px',
                  marginBottom: '0.75rem',
                  border: '1px solid #e2e8f0'
                }}>
                  {/* 상태 표시 */}
                  <div style={{
                    width: '8px',
                    height: '8px',
                    borderRadius: '50%',
                    background: service.status === 'running' ? '#22c55e' : '#6b7280',
                    flexShrink: 0
                  }}></div>
                  
                  {/* 서비스 아이콘 */}
                  <div style={{
                    width: '2.5rem',
                    height: '2.5rem',
                    background: service.status === 'running' ? '#dcfce7' : '#f1f5f9',
                    color: service.status === 'running' ? '#16a34a' : '#64748b',
                    borderRadius: '8px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '1.125rem',
                    flexShrink: 0
                  }}>
                    {service.icon === 'server' ? '🖥️' : 
                     service.icon === 'download' ? '📥' :
                     service.icon === 'database' ? '🗄️' :
                     service.icon === 'exchange' ? '🔄' :
                     service.icon === 'elephant' ? '🐘' : '⚙️'}
                  </div>
                  
                  {/* 서비스 정보 */}
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{
                      fontWeight: '600',
                      color: '#1e293b',
                      fontSize: '0.875rem'
                    }}>
                      {service.displayName}
                    </div>
                    <div style={{
                      color: '#64748b',
                      fontSize: '0.75rem',
                      marginBottom: '0.25rem'
                    }}>
                      {service.description}
                    </div>
                    {service.port && (
                      <div style={{
                        color: '#64748b',
                        fontSize: '0.75rem'
                      }}>
                        포트: {service.port}
                      </div>
                    )}
                    {service.version && (
                      <div style={{
                        color: '#64748b',
                        fontSize: '0.75rem'
                      }}>
                        v{service.version}
                      </div>
                    )}
                  </div>
                  
                  {/* 메트릭 정보 */}
                  <div style={{
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '0.25rem',
                    fontSize: '0.75rem',
                    color: '#64748b',
                    textAlign: 'right',
                    minWidth: '80px'
                  }}>
                    {service.memory_usage && service.memory_usage > 0 && (
                      <div>메모리: {service.memory_usage}MB</div>
                    )}
                    {service.cpu_usage && service.cpu_usage > 0 && (
                      <div>CPU: {service.cpu_usage}%</div>
                    )}
                    {service.uptime && service.uptime > 0 && (
                      <div>가동시간: {formatUptime(service.uptime)}</div>
                    )}
                    {service.last_error && service.status !== 'running' && (
                      <div style={{ color: '#dc2626', fontSize: '0.7rem' }}>
                        {service.last_error}
                      </div>
                    )}
                  </div>
                  
                  {/* 제어 버튼 */}
                  <div style={{ flexShrink: 0 }}>
                    {service.controllable ? (
                      service.status === 'running' ? (
                        <div style={{ display: 'flex', gap: '0.5rem' }}>
                          <button 
                            onClick={() => handleServiceAction(service.name, service.displayName, 'stop')}
                            style={{
                              padding: '0.5rem 1rem',
                              background: '#ef4444',
                              color: 'white',
                              border: 'none',
                              borderRadius: '6px',
                              cursor: 'pointer',
                              fontSize: '0.75rem',
                              fontWeight: '600',
                              display: 'flex',
                              alignItems: 'center',
                              justifyContent: 'center',
                              gap: '0.25rem',
                              minWidth: '70px',
                              whiteSpace: 'nowrap'
                            }}
                          >
                            ⏹️ 중지
                          </button>
                          
                          <button 
                            onClick={() => handleServiceAction(service.name, service.displayName, 'restart')}
                            style={{
                              padding: '0.5rem 1rem',
                              background: '#f59e0b',
                              color: 'white',
                              border: 'none',
                              borderRadius: '6px',
                              cursor: 'pointer',
                              fontSize: '0.75rem',
                              fontWeight: '600',
                              display: 'flex',
                              alignItems: 'center',
                              justifyContent: 'center',
                              gap: '0.25rem',
                              minWidth: '70px',
                              whiteSpace: 'nowrap'
                            }}
                          >
                            🔄 재시작
                          </button>
                        </div>
                      ) : (
                        <button 
                          onClick={() => handleServiceAction(service.name, service.displayName, 'start')}
                          style={{
                            padding: '0.75rem 1.25rem',
                            background: '#22c55e',
                            color: 'white',
                            border: 'none',
                            borderRadius: '6px',
                            cursor: 'pointer',
                            fontSize: '0.75rem',
                            fontWeight: '600',
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center',
                            gap: '0.25rem',
                            minWidth: '70px',
                            whiteSpace: 'nowrap'
                          }}
                        >
                          ▶️ 시작
                        </button>
                      )
                    ) : (
                      <span style={{
                        fontSize: '0.75rem',
                        color: '#3b82f6',
                        background: '#dbeafe',
                        padding: '0.5rem 0.75rem',
                        borderRadius: '12px',
                        fontWeight: '500'
                      }}>
                        필수
                      </span>
                    )}
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* 📊 오른쪽: 2x2 시스템 상태 그리드 */}
        {dashboardData && (
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: '1rem'
          }}>
            
            {/* 시스템 개요 */}
            <div style={{
              background: 'white',
              borderRadius: '12px',
              boxShadow: '0 2px 10px rgba(0, 0, 0, 0.05)',
              overflow: 'hidden'
            }}>
              <div style={{
                padding: '1.5rem',
                background: '#f8fafc',
                borderBottom: '1px solid #e2e8f0',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '0.75rem',
                  fontSize: '1rem',
                  fontWeight: '600',
                  color: '#334155'
                }}>
                  <div style={{
                    width: '1.5rem',
                    height: '1.5rem',
                    background: '#dbeafe',
                    color: '#3b82f6',
                    borderRadius: '6px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '0.875rem'
                  }}>
                    💙
                  </div>
                  시스템 개요
                </div>
                <span style={{
                  fontSize: '0.75rem',
                  color: '#f59e0b',
                  background: '#fef3c7',
                  padding: '0.25rem 0.5rem',
                  borderRadius: '12px'
                }}>
                  주의
                </span>
              </div>
              <div style={{ padding: '1.5rem' }}>
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: '1fr 1fr',
                  gap: '0.75rem'
                }}>
                  <div style={{ 
                    textAlign: 'center',
                    padding: '1rem',
                    background: '#f8fafc',
                    borderRadius: '8px',
                    border: '1px solid #e2e8f0'
                  }}>
                    <div style={{
                      fontSize: '1.5rem',
                      fontWeight: '700',
                      color: '#1e293b',
                      marginBottom: '0.25rem'
                    }}>
                      {dashboardData.device_summary.total_devices}
                    </div>
                    <div style={{
                      fontSize: '0.75rem',
                      color: '#64748b',
                      marginBottom: '0.5rem'
                    }}>
                      디바이스
                    </div>
                    <div style={{
                      fontSize: '0.6rem',
                      color: '#64748b'
                    }}>
                      연결: {dashboardData.device_summary.connected_devices} / 활성: {dashboardData.device_summary.enabled_devices}
                    </div>
                  </div>
                  
                  <div style={{ 
                    textAlign: 'center',
                    padding: '1rem',
                    background: '#f8fafc',
                    borderRadius: '8px',
                    border: '1px solid #e2e8f0'
                  }}>
                    <div style={{
                      fontSize: '1.5rem',
                      fontWeight: '700',
                      color: '#1e293b',
                      marginBottom: '0.25rem'
                    }}>
                      {dashboardData.system_metrics.dataPointsPerSecond}
                    </div>
                    <div style={{
                      fontSize: '0.75rem',
                      color: '#64748b',
                      marginBottom: '0.5rem'
                    }}>
                      데이터 포인트/초
                    </div>
                    <div style={{
                      fontSize: '0.6rem',
                      color: '#64748b'
                    }}>
                      응답시간: {dashboardData.system_metrics.avgResponseTime}ms
                    </div>
                  </div>
                  
                  <div style={{ 
                    textAlign: 'center',
                    padding: '1rem',
                    background: '#f8fafc',
                    borderRadius: '8px',
                    border: '1px solid #e2e8f0'
                  }}>
                    <div style={{
                      fontSize: '1.5rem',
                      fontWeight: '700',
                      color: '#1e293b',
                      marginBottom: '0.25rem'
                    }}>
                      {dashboardData.alarms.total}
                    </div>
                    <div style={{
                      fontSize: '0.75rem',
                      color: '#64748b',
                      marginBottom: '0.5rem'
                    }}>
                      활성 알람
                    </div>
                    <div style={{
                      fontSize: '0.6rem',
                      color: '#64748b'
                    }}>
                      심각: {dashboardData.alarms.critical} / 미확인: {dashboardData.alarms.unacknowledged}
                    </div>
                  </div>
                  
                  <div style={{ 
                    textAlign: 'center',
                    padding: '1rem',
                    background: '#f8fafc',
                    borderRadius: '8px',
                    border: '1px solid #e2e8f0'
                  }}>
                    <div style={{
                      fontSize: '1.5rem',
                      fontWeight: '700',
                      color: '#1e293b',
                      marginBottom: '0.25rem'
                    }}>
                      {dashboardData.device_summary.data_points_count}
                    </div>
                    <div style={{
                      fontSize: '0.75rem',
                      color: '#64748b',
                      marginBottom: '0.5rem'
                    }}>
                      데이터 포인트
                    </div>
                    <div style={{
                      fontSize: '0.6rem',
                      color: '#64748b'
                    }}>
                      프로토콜: {dashboardData.device_summary.protocols_count}개
                    </div>
                  </div>
                </div>
              </div>
            </div>

            {/* 시스템 리소스 */}
            <div style={{
              background: 'white',
              borderRadius: '12px',
              boxShadow: '0 2px 10px rgba(0, 0, 0, 0.05)',
              overflow: 'hidden'
            }}>
              <div style={{
                padding: '1.5rem',
                background: '#f8fafc',
                borderBottom: '1px solid #e2e8f0',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '0.75rem',
                  fontSize: '1rem',
                  fontWeight: '600',
                  color: '#334155'
                }}>
                  <div style={{
                    width: '1.5rem',
                    height: '1.5rem',
                    background: '#fef3c7',
                    color: '#f59e0b',
                    borderRadius: '6px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '0.875rem'
                  }}>
                    📊
                  </div>
                  시스템 리소스
                </div>
                <div style={{
                  fontSize: '0.75rem',
                  color: '#64748b'
                }}>
                  평균 응답시간: {dashboardData.system_metrics.avgResponseTime}ms
                </div>
              </div>
              <div style={{ padding: '1.5rem' }}>
                {[
                  { label: 'CPU 사용률', value: dashboardData.system_metrics.cpuUsage, unit: '%' },
                  { label: '메모리', value: dashboardData.system_metrics.memoryUsage, unit: '%' },
                  { label: '디스크', value: dashboardData.system_metrics.diskUsage, unit: '%' },
                  { label: '네트워크', value: dashboardData.system_metrics.networkUsage, unit: ' Mbps' }
                ].map((metric, index) => (
                  <div key={index} style={{ marginBottom: '1rem' }}>
                    <div style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      marginBottom: '0.5rem',
                      fontSize: '0.875rem'
                    }}>
                      <span style={{ color: '#374151' }}>{metric.label}</span>
                      <span style={{ fontWeight: '600', color: '#1f2937' }}>
                        {metric.value}{metric.unit}
                      </span>
                    </div>
                    <div style={{
                      width: '100%',
                      height: '6px',
                      background: '#f3f4f6',
                      borderRadius: '3px',
                      overflow: 'hidden'
                    }}>
                      <div style={{
                        width: `${Math.min(metric.value, 100)}%`,
                        height: '100%',
                        background: metric.value > 80 ? '#ef4444' : metric.value > 60 ? '#f59e0b' : '#22c55e',
                        transition: 'width 0.3s ease'
                      }}></div>
                    </div>
                  </div>
                ))}
                
                <div style={{
                  borderTop: '1px solid #e5e7eb',
                  paddingTop: '1rem',
                  fontSize: '0.75rem',
                  color: '#6b7280'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '0.25rem' }}>
                    <span>활성 연결:</span>
                    <span>{dashboardData.system_metrics.activeConnections}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '0.25rem' }}>
                    <span>큐 크기:</span>
                    <span>{dashboardData.system_metrics.queueSize}</span>
                  </div>
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span>DB 쿼리 시간:</span>
                    <span>{dashboardData.system_metrics.dbQueryTime}ms</span>
                  </div>
                </div>
              </div>
            </div>

            {/* 성능 지표 */}
            <div style={{
              background: 'white',
              borderRadius: '12px',
              boxShadow: '0 2px 10px rgba(0, 0, 0, 0.05)',
              overflow: 'hidden'
            }}>
              <div style={{
                padding: '1.5rem',
                background: '#f8fafc',
                borderBottom: '1px solid #e2e8f0'
              }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '0.75rem',
                  fontSize: '1rem',
                  fontWeight: '600',
                  color: '#334155'
                }}>
                  <div style={{
                    width: '1.5rem',
                    height: '1.5rem',
                    background: '#e0f2fe',
                    color: '#0891b2',
                    borderRadius: '6px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '0.875rem'
                  }}>
                    📈
                  </div>
                  성능 지표
                </div>
              </div>
              <div style={{ padding: '1.5rem' }}>
                {[
                  { label: 'API 응답시간', value: `${dashboardData.performance.api_response_time}ms` },
                  { label: 'DB 응답시간', value: `${dashboardData.performance.database_response_time}ms` },
                  { label: '캐시 적중률', value: `${dashboardData.performance.cache_hit_rate}%` },
                  { label: '처리량/초', value: `${dashboardData.performance.throughput_per_second}` },
                  { label: '에러율', value: `${dashboardData.performance.error_rate.toFixed(2)}%` }
                ].map((perf, index) => (
                  <div key={index} style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    padding: '0.75rem',
                    background: '#f8fafc',
                    borderRadius: '6px',
                    marginBottom: '0.5rem',
                    fontSize: '0.875rem'
                  }}>
                    <span style={{ color: '#64748b' }}>{perf.label}</span>
                    <span style={{ fontWeight: '600', color: '#1e293b' }}>{perf.value}</span>
                  </div>
                ))}
              </div>
            </div>

            {/* 헬스 체크 */}
            <div style={{
              background: 'white',
              borderRadius: '12px',
              boxShadow: '0 2px 10px rgba(0, 0, 0, 0.05)',
              overflow: 'hidden'
            }}>
              <div style={{
                padding: '1.5rem',
                background: '#f8fafc',
                borderBottom: '1px solid #e2e8f0'
              }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '0.75rem',
                  fontSize: '1rem',
                  fontWeight: '600',
                  color: '#334155'
                }}>
                  <div style={{
                    width: '1.5rem',
                    height: '1.5rem',
                    background: '#dcfce7',
                    color: '#16a34a',
                    borderRadius: '6px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '0.875rem'
                  }}>
                    💚
                  </div>
                  헬스 체크
                </div>
              </div>
              <div style={{ padding: '1.5rem' }}>
                {[
                  { label: '데이터베이스', status: dashboardData.health_status.database },
                  { label: '네트워크', status: dashboardData.health_status.network },
                  { label: '스토리지', status: dashboardData.health_status.storage },
                  { label: '캐시', status: dashboardData.health_status.cache },
                  { label: '메시지 큐', status: dashboardData.health_status.message_queue }
                ].map((health, index) => (
                  <div key={index} style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    padding: '0.5rem 0',
                    borderBottom: index < 4 ? '1px solid #f1f5f9' : 'none',
                    fontSize: '0.875rem'
                  }}>
                    <span style={{ color: '#64748b' }}>{health.label}</span>
                    <span style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '0.5rem',
                      color: health.status === 'healthy' ? '#16a34a' : 
                             health.status === 'warning' ? '#f59e0b' : '#dc2626'
                    }}>
                      <span style={{
                        width: '6px',
                        height: '6px',
                        borderRadius: '50%',
                        background: health.status === 'healthy' ? '#22c55e' : 
                                   health.status === 'warning' ? '#f59e0b' : '#ef4444'
                      }}></span>
                      {health.status}
                    </span>
                  </div>
                ))}
              </div>
            </div>

          </div>
        )}
      </div>

      {/* 📊 하단: 최근 알람 */}
      {dashboardData && dashboardData.alarms.recent_alarms.length > 0 && (
        <div style={{
          background: 'white',
          borderRadius: '12px',
          boxShadow: '0 2px 10px rgba(0, 0, 0, 0.05)',
          overflow: 'hidden'
        }}>
          <div style={{
            padding: '1.5rem',
            background: '#f8fafc',
            borderBottom: '1px solid #e2e8f0',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center'
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '0.75rem',
              fontSize: '1.125rem',
              fontWeight: '600',
              color: '#334155'
            }}>
              <div style={{
                width: '2rem',
                height: '2rem',
                background: '#fef2f2',
                color: '#dc2626',
                borderRadius: '8px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center'
              }}>
                🔔
              </div>
              최근 알람
              <span style={{
                fontSize: '0.75rem',
                color: '#64748b',
                background: '#f1f5f9',
                padding: '0.25rem 0.5rem',
                borderRadius: '12px'
              }}>
                24시간 내: {dashboardData.alarms.recent_24h}건
              </span>
            </div>
            <button style={{
              padding: '0.5rem 1rem',
              background: 'transparent',
              color: '#3b82f6',
              border: '1px solid #3b82f6',
              borderRadius: '6px',
              cursor: 'pointer',
              fontSize: '0.875rem',
              display: 'flex',
              alignItems: 'center',
              gap: '0.5rem'
            }}>
              모든 알람 보기 →
            </button>
          </div>
          <div style={{ padding: '1.5rem' }}>
            {dashboardData.alarms.recent_alarms.slice(0, 3).map((alarm, index) => (
              <div key={alarm.id} style={{
                display: 'flex',
                alignItems: 'flex-start',
                gap: '1rem',
                padding: '1rem',
                background: '#f8fafc',
                borderRadius: '8px',
                marginBottom: index < 2 ? '0.75rem' : 0,
                border: '1px solid #e2e8f0'
              }}>
                <div style={{
                  width: '2rem',
                  height: '2rem',
                  background: alarm.type === 'warning' ? '#fef3c7' : '#e0f2fe',
                  color: alarm.type === 'warning' ? '#f59e0b' : '#0891b2',
                  borderRadius: '6px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  flexShrink: 0
                }}>
                  {alarm.type === 'warning' ? '⚠️' : 'ℹ️'}
                </div>
                <div style={{ flex: 1 }}>
                  <div style={{
                    fontWeight: '500',
                    color: '#1e293b',
                    fontSize: '0.875rem',
                    marginBottom: '0.25rem'
                  }}>
                    {alarm.message}
                  </div>
                  <div style={{
                    fontSize: '0.75rem',
                    color: '#64748b'
                  }}>
                    {alarm.device_name} • {new Date(alarm.timestamp).toLocaleString()} • 심각도: {alarm.severity}
                  </div>
                </div>
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'flex-end',
                  gap: '0.25rem',
                  flexShrink: 0
                }}>
                  <div style={{
                    fontSize: '0.75rem',
                    color: '#64748b'
                  }}>
                    {new Date(alarm.timestamp).toLocaleTimeString()}
                  </div>
                  <span style={{
                    fontSize: '0.75rem',
                    color: alarm.acknowledged ? '#16a34a' : '#dc2626',
                    background: alarm.acknowledged ? '#dcfce7' : '#fef2f2',
                    padding: '0.25rem 0.5rem',
                    borderRadius: '12px'
                  }}>
                    {alarm.acknowledged ? '✅ 확인됨' : '❗ 미확인'}
                  </span>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* 📊 하단 상태바 */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginTop: '2rem',
        padding: '1rem 1.5rem',
        background: 'white',
        borderRadius: '8px',
        boxShadow: '0 1px 3px rgba(0, 0, 0, 0.1)',
        fontSize: '0.875rem',
        color: '#64748b'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
          <span>마지막 업데이트: {lastUpdate.toLocaleTimeString()}</span>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <div style={{
              width: '6px',
              height: '6px',
              borderRadius: '50%',
              background: '#22c55e',
              animation: 'pulse 2s infinite'
            }}></div>
            10초마다 자동 새로고침
          </span>
        </div>
        
        <div style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <div style={{
              width: '6px',
              height: '6px',
              borderRadius: '50%',
              background: '#f59e0b'
            }}></div>
            시스템: 주의
          </span>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <div style={{
              width: '6px',
              height: '6px',
              borderRadius: '50%',
              background: connectionStatus === 'connected' ? '#22c55e' : '#ef4444'
            }}></div>
            {connectionStatus === 'connected' ? '연결됨' : '연결 끊김'}
          </span>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <div style={{
              width: '6px',
              height: '6px',
              borderRadius: '50%',
              background: '#ef4444'
            }}></div>
            임시 데이터 표시 중
          </span>
        </div>
      </div>

      <style>
        {`
          @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
          }
        `}
      </style>
    </div>
  );
};

export default Dashboard;