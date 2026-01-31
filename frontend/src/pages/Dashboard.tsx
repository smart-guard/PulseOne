import React, { useState, useEffect, useCallback } from 'react';
import { DashboardApiService, DashboardOverviewData, ServiceInfo, SystemMetrics, DeviceSummary, AlarmSummary, RecentAlarm } from '../api/services/dashboardApi';
import { Pagination } from '../components/common/Pagination';

// ============================================================================
// 📋 타입 정의 (DashboardApiService와 일치)
// ============================================================================

interface HealthStatus {
  overall: 'healthy' | 'degraded' | 'critical';
  database: 'healthy' | 'warning' | 'critical';
  redis: 'healthy' | 'warning' | 'critical';
  collector: 'healthy' | 'warning' | 'critical';
  gateway: 'healthy' | 'warning' | 'critical';
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
  health_status: HealthStatus;
  performance: PerformanceMetrics;
  last_updated: string;
}

// 팝업 확인 다이얼로그 인터페이스
interface ConfirmDialogState {
  isOpen: boolean;
  title: string;
  message: string;
  confirmText: string;
  cancelText: string;
  onConfirm: () => void;
  onCancel: () => void;
  type: 'warning' | 'danger' | 'info';
}

interface TodayAlarmStats {
  today_total: number;
  severity_breakdown: {
    critical: number;
    major: number;
    minor: number;
    warning: number;
  };
  hourly_distribution: Array<{
    hour: number;
    count: number;
  }>;
  device_breakdown: Array<{
    device_id: number;
    device_name: string;
    alarm_count: number;
  }>;
}

// ============================================================================
// 🧩 하위 컴포넌트들
// ============================================================================

// 요약 타일 컴포넌트
const SummaryTile: React.FC<{
  title: string;
  value: string | number;
  subValue?: string;
  unit?: string;
  icon: string;
  color: string;
  trend?: { value: number; label: string; positive: boolean };
}> = ({ title, value, subValue, unit, icon, color, trend }) => (
  <div style={{
    background: 'white',
    borderRadius: '16px',
    padding: '24px',
    boxShadow: '0 4px 20px rgba(0, 0, 0, 0.05)',
    border: `1px solid #f1f5f9`,
    display: 'flex',
    flexDirection: 'column',
    gap: '12px'
  }}>
    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
      <div style={{
        width: '48px',
        height: '48px',
        borderRadius: '12px',
        background: `${color}15`,
        color: color,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        fontSize: '24px'
      }}>
        {icon}
      </div>
      {trend && (
        <div style={{
          fontSize: '12px',
          fontWeight: '600',
          color: trend.positive ? '#10b981' : '#ef4444',
          background: trend.positive ? '#ecfdf5' : '#fef2f2',
          padding: '4px 8px',
          borderRadius: '20px',
          display: 'flex',
          alignItems: 'center',
          gap: '4px'
        }}>
          {trend.positive ? '↑' : '↓'} {trend.label}
        </div>
      )}
    </div>
    <div>
      <div style={{ fontSize: '14px', color: '#64748b', fontWeight: '500', marginBottom: '4px' }}>{title}</div>
      <div style={{ display: 'flex', alignItems: 'baseline', gap: '4px' }}>
        <span style={{ fontSize: '28px', fontWeight: '700', color: '#1e293b' }}>{value}</span>
        {unit && <span style={{ fontSize: '14px', color: '#94a3b8', fontWeight: '500' }}>{unit}</span>}
      </div>
      {subValue && <div style={{ fontSize: '12px', color: '#94a3b8', marginTop: '4px' }}>{subValue}</div>}
    </div>
  </div>
);

// Flow Monitor 시각화 컴포넌트
const FlowMonitor: React.FC<{ data: DashboardData | null }> = ({ data }) => {
  if (!data) return null;

  const steps = [
    {
      id: 'source',
      label: '현공 데이터 수집',
      icon: '📡',
      status: 'healthy',
      value: `${data.system_metrics.dataPointsPerSecond} PPS`,
      subValue: '데이터 유입량'
    },
    {
      id: 'collector',
      label: '커넥터 처리',
      icon: '📥',
      status: data.health_status.collector,
      value: `${data.communication_status.upstream.connectivity_rate}%`,
      subValue: '디바이스 통신율'
    },
    {
      id: 'gateway',
      label: '내보내기 게이트웨이',
      icon: '🔄',
      status: data.health_status.gateway,
      value: `${data.services.details.filter(s => s.name.includes('gateway') && s.status === 'running').length} Active`,
      subValue: '활성 서비스'
    },
    {
      id: 'target',
      label: '최종 목적지 전송',
      icon: '🎯',
      status: data.communication_status.downstream.success_rate > 90 ? 'healthy' : 'warning',
      value: `${data.communication_status.downstream.success_rate}%`,
      subValue: '전송 성공률'
    }
  ];

  return (
    <div style={{
      background: 'white',
      borderRadius: '20px',
      padding: '32px 40px',
      boxShadow: '0 10px 30px rgba(0, 0, 0, 0.04)',
      marginBottom: '32px',
      border: '1px solid #f1f5f9',
      position: 'relative',
      overflow: 'hidden'
    }}>
      {/* 장식용 배경 */}
      <div style={{
        position: 'absolute',
        top: 0,
        right: 0,
        width: '300px',
        height: '100%',
        background: 'linear-gradient(90deg, rgba(255,255,255,0) 0%, rgba(248,250,252,1) 100%)',
        zIndex: 0
      }}></div>

      <div style={{ position: 'relative', zIndex: 1 }}>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'baseline',
          marginBottom: '32px'
        }}>
          <div>
            <div style={{ fontSize: '20px', fontWeight: '800', color: '#0f172a', letterSpacing: '-0.025em' }}>운영 데이터 흐름 모니터링</div>
            <div style={{ fontSize: '13px', color: '#64748b', marginTop: '4px' }}>수집된 로우 데이터가 최종 목적지까지 전달되는 전 과정을 실시간 모니터링합니다.</div>
          </div>
          <div style={{ fontSize: '12px', fontWeight: '700', color: '#94a3b8', background: '#f8fafc', padding: '6px 12px', borderRadius: '8px', border: '1px solid #f1f5f9' }}>
            시스템 건전성: {data.health_status.overall === 'healthy' ? '🟢 최상' : '🟡 주의'}
          </div>
        </div>

        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          position: 'relative',
          padding: '0 20px'
        }}>
          {steps.map((step, index) => (
            <React.Fragment key={step.id}>
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                gap: '16px',
                zIndex: 1,
                width: '160px'
              }}>
                <div style={{
                  width: '72px',
                  height: '72px',
                  borderRadius: '24px',
                  background: 'white',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  fontSize: '32px',
                  boxShadow: '0 8px 20px rgba(0, 0, 0, 0.06)',
                  border: `2px solid ${step.status === 'healthy' ? '#10b981' : step.status === 'warning' ? '#f59e0b' : '#ef4444'}`,
                  position: 'relative'
                }}>
                  {step.icon}
                  <div style={{
                    position: 'absolute',
                    top: '-4px',
                    right: '-4px',
                    width: '16px',
                    height: '16px',
                    borderRadius: '50%',
                    background: step.status === 'healthy' ? '#10b981' : step.status === 'warning' ? '#f59e0b' : '#ef4444',
                    border: '3px solid white',
                    boxShadow: '0 2px 4px rgba(0,0,0,0.1)'
                  }}></div>
                </div>
                <div style={{ textAlign: 'center' }}>
                  <div style={{ fontSize: '14px', fontWeight: '800', color: '#1e293b', marginBottom: '4px', whiteSpace: 'nowrap' }}>{step.label}</div>
                  <div style={{ fontSize: '18px', fontWeight: '900', color: step.status === 'healthy' ? '#059669' : '#1e293b' }}>{step.value}</div>
                  <div style={{ fontSize: '11px', color: '#94a3b8', fontWeight: '600', textTransform: 'uppercase', marginTop: '2px' }}>{step.subValue}</div>
                </div>
              </div>

              {index < steps.length - 1 && (
                <div style={{
                  flex: 1,
                  height: '2px',
                  background: `linear-gradient(90deg, ${step.status === 'healthy' ? '#10b981' : '#cbd5e1'} 0%, ${steps[index + 1].status === 'healthy' ? '#10b981' : '#cbd5e1'} 100%)`,
                  margin: '0 -10px',
                  marginBottom: '50px',
                  position: 'relative',
                  opacity: 0.6
                }}>
                  <div style={{
                    position: 'absolute',
                    top: '-4px',
                    left: '50%',
                    transform: 'translateX(-50%)',
                    fontSize: '14px',
                    color: '#94a3b8',
                    background: 'white',
                    padding: '0 8px'
                  }}>▶</div>
                </div>
              )}
            </React.Fragment>
          ))}
        </div>
      </div>
    </div>
  );
};

// ============================================================================
// 🎯 메인 대시보드 컴포넌트
// ============================================================================

const Dashboard: React.FC = () => {
  // ==========================================================================
  // 📊 상태 관리
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

  // 성공 메시지 상태
  const [successMessage, setSuccessMessage] = useState<string | null>(null);
  const [processing, setProcessing] = useState<string | null>(null);

  // 유틸리티 함수
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

  // ==========================================================================
  // 🔄 API 호출 및 데이터 로드
  // ==========================================================================

  /**
   * 대시보드 개요 데이터 로드 (하나의 통합 API 사용)
   */
  const loadDashboardOverview = useCallback(async (showLoading = false) => {
    try {
      if (showLoading) {
        setIsLoading(true);
      }
      setError(null);

      console.log('🎯 통합 대시보드 데이터 로드 시작...');

      // 단일 통합 API 호출
      const response = await DashboardApiService.getOverview();

      if (response.success && response.data) {
        // 백엔드 데이터를 프런트엔드 형식에 맞게 변환 및 보정
        const transformedData = transformApiDataToDashboard(response.data);
        setDashboardData(transformedData);
        setConnectionStatus('connected');
        setConsecutiveErrors(0);
        console.log('✅ 대시보드 통합 데이터 로드 및 변환 완료');
      } else {
        throw new Error(response.message || '데이터 로드 실패');
      }

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
   * API 데이터를 대시보드 형식으로 변환 (현재는 getOverview에서 직접 리턴하므로 사용처 확인 후 정리)
   * ⚠️ 더 이상 사용되지 않거나 단순 매핑용으로 변경될 예정입니다.
   */
  const transformApiDataToDashboard = (apiData: any): DashboardData => {
    if (!apiData) return createFallbackDashboardData();

    const now = new Date();

    // 서비스 목록 보정 (필수 필드 및 아이콘 매핑)
    const servicesDetails = (apiData.services?.details || []).map((s: any) => ({
      ...s,
      icon: s.icon || (s.name.includes('collector') ? 'download' :
        s.name.includes('gateway') ? 'exchange' : 'server'),
      status: s.status || 'stopped',
      displayName: s.displayName || s.name,
      controllable: s.controllable !== undefined ? s.controllable : true
    }));

    return {
      services: {
        total: apiData.services?.total || 0,
        running: apiData.services?.running || 0,
        stopped: apiData.services?.stopped || 0,
        error: apiData.services?.error || 0,
        details: servicesDetails
      },
      system_metrics: {
        dataPointsPerSecond: apiData.system_metrics?.dataPointsPerSecond || 0,
        avgResponseTime: apiData.system_metrics?.avgResponseTime || 0,
        dbQueryTime: apiData.system_metrics?.dbQueryTime || 0,
        cpuUsage: apiData.system_metrics?.cpuUsage || apiData.system_metrics?.cpu?.usage || 0,
        memoryUsage: apiData.system_metrics?.memoryUsage || apiData.system_metrics?.memory?.usage || 0,
        diskUsage: apiData.system_metrics?.diskUsage || apiData.system_metrics?.disk?.usage || 0,
        networkUsage: apiData.system_metrics?.networkUsage || 0,
        activeConnections: apiData.system_metrics?.activeConnections || 0,
        queueSize: apiData.system_metrics?.queueSize || 0,
        timestamp: apiData.system_metrics?.timestamp || now.toISOString()
      },
      device_summary: {
        total_devices: apiData.device_summary?.total || apiData.device_summary?.total_devices || 0,
        connected_devices: apiData.device_summary?.connected || apiData.device_summary?.connected_devices || 0,
        disconnected_devices: apiData.device_summary?.disconnected || apiData.device_summary?.disconnected_devices || 0,
        error_devices: apiData.device_summary?.error || apiData.device_summary?.error_devices || 0,
        protocols_count: apiData.device_summary?.protocols_count || 0,
        sites_count: apiData.device_summary?.sites_count || 0,
        data_points_count: apiData.device_summary?.data_points_count || 0,
        enabled_devices: apiData.device_summary?.enabled_devices || 0
      },
      alarms: {
        active_total: apiData.alarms?.active_total || 0,
        today_total: apiData.alarms?.today_total || 0,
        unacknowledged: apiData.alarms?.unacknowledged || 0,
        critical: apiData.alarms?.critical || 0,
        major: apiData.alarms?.major || 0,
        minor: apiData.alarms?.minor || 0,
        warning: apiData.alarms?.warning || 0,
        recent_alarms: apiData.alarms?.recent_alarms || []
      },
      communication_status: {
        upstream: {
          total_devices: apiData.communication_status?.upstream?.total_devices || 0,
          connected_devices: apiData.communication_status?.upstream?.connected_devices || 0,
          connectivity_rate: apiData.communication_status?.upstream?.connectivity_rate || 0,
          last_polled_at: apiData.communication_status?.upstream?.last_polled_at || now.toISOString()
        },
        downstream: {
          total_exports: apiData.communication_status?.downstream?.total_exports || 0,
          success_rate: apiData.communication_status?.downstream?.success_rate || 0,
          last_dispatch_at: apiData.communication_status?.downstream?.last_dispatch_at || now.toISOString()
        }
      },
      health_status: {
        overall: apiData.health_status?.overall || 'degraded',
        database: apiData.health_status?.database || 'warning',
        redis: apiData.health_status?.redis || 'critical',
        collector: apiData.health_status?.collector || 'warning',
        gateway: apiData.health_status?.gateway || 'warning',
        network: apiData.health_status?.network || 'healthy',
        storage: apiData.health_status?.storage || 'healthy',
        cache: apiData.health_status?.cache || 'warning',
        message_queue: apiData.health_status?.message_queue || 'warning'
      },
      performance: {
        api_response_time: apiData.performance?.api_response_time || 0,
        database_response_time: apiData.performance?.database_response_time || 0,
        cache_hit_rate: apiData.performance?.cache_hit_rate || 0,
        error_rate: apiData.performance?.error_rate || 0,
        throughput_per_second: apiData.performance?.throughput_per_second || 0
      },
      last_updated: apiData.last_updated || now.toISOString()
    };
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
            description: '백엔드 서비스',
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
        active_total: 0,
        today_total: 0,
        unacknowledged: 0,
        critical: 0,
        major: 0,
        minor: 0,
        warning: 0,
        recent_alarms: []
      },
      communication_status: {
        upstream: {
          total_devices: 0,
          connected_devices: 0,
          connectivity_rate: 0,
          last_polled_at: now.toISOString()
        },
        downstream: {
          total_exports: 0,
          success_rate: 0,
          last_dispatch_at: now.toISOString()
        }
      },
      health_status: {
        overall: 'degraded',
        database: 'warning',
        redis: 'critical',
        collector: 'warning',
        gateway: 'warning',
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

  // ==========================================================================
  // 🛠️ 서비스 제어 함수들 (DashboardApiService 사용)
  // ==========================================================================

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
        executeServiceAction(serviceName, displayName, action as any);
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

  const executeServiceAction = async (serviceName: string, displayName: string, action: 'start' | 'stop' | 'restart') => {
    try {
      setProcessing(serviceName);
      console.log(`🔧 ${serviceName} ${action} 실행중...`);

      // DashboardApiService를 사용한 실제 API 호출
      const response = await DashboardApiService.controlService(serviceName, action);

      if (response.success) {
        setSuccessMessage(`${displayName}이(가) 성공적으로 ${action} 되었습니다.`);
        // 상태 업데이트를 위한 데이터 새로고침
        setTimeout(() => {
          loadDashboardOverview(false);
        }, 2000); // 서비스 상태 변경 대기
      } else {
        throw new Error(response.data?.error || response.error || `${action} 실패`);
      }
    } catch (error) {
      console.error(`❌ ${serviceName} ${action} 실패:`, error);
      const errorMessage = error instanceof Error ? error.message : `${displayName} ${action} 실패`;
      setError(errorMessage);
    } finally {
      setProcessing(null);
    }
  };

  // ==========================================================================
  // 🔄 자동 새로고침 및 라이프사이클
  // ==========================================================================

  useEffect(() => {
    loadDashboardOverview(true);
  }, [loadDashboardOverview]);

  // 자동 새로고침
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      if (!isLoading) {
        loadDashboardOverview(false);
      }
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, isLoading, loadDashboardOverview]);

  // 성공 메시지 자동 제거
  useEffect(() => {
    if (successMessage) {
      const timer = setTimeout(() => setSuccessMessage(null), 3000);
      return () => clearTimeout(timer);
    }
  }, [successMessage]);

  // ==========================================================================
  // 🎨 컴포넌트 렌더링
  // ==========================================================================

  // 확인 다이얼로그 컴포넌트
  const ConfirmDialog: React.FC<{ config: any }> = ({ config }) => {
    if (!config?.show) return null;

    const getDialogColor = (type: string) => {
      switch (type) {
        case 'danger': return '#ef4444';
        case 'warning': return '#f59e0b';
        case 'info':
        default: return '#3b82f6';
      }
    };

    return (
      <div style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.5)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 10000
      }}>
        <div style={{
          backgroundColor: 'white',
          borderRadius: '12px',
          padding: '24px',
          minWidth: '400px',
          maxWidth: '500px',
          boxShadow: '0 25px 50px rgba(0, 0, 0, 0.25)'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
            marginBottom: '16px'
          }}>
            <div style={{
              width: '40px',
              height: '40px',
              borderRadius: '50%',
              backgroundColor: `${getDialogColor(config.type)}20`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '20px'
            }}>
              {config.type === 'warning' ? '⚠️' : config.type === 'danger' ? '🚨' : 'ℹ️'}
            </div>
            <h3 style={{
              margin: 0,
              fontSize: '18px',
              fontWeight: '600',
              color: '#1e293b'
            }}>
              {config.title}
            </h3>
          </div>

          <p style={{
            margin: 0,
            marginBottom: '24px',
            fontSize: '14px',
            color: '#64748b',
            lineHeight: '1.5',
            whiteSpace: 'pre-line'
          }}>
            {config.message}
          </p>

          <div style={{
            display: 'flex',
            justifyContent: 'flex-end',
            gap: '12px'
          }}>
            <button
              onClick={() => setConfirmModal(null)}
              style={{
                padding: '10px 20px',
                backgroundColor: '#f3f4f6',
                border: '1px solid #d1d5db',
                borderRadius: '6px',
                fontSize: '14px',
                fontWeight: '500',
                color: '#374151',
                cursor: 'pointer'
              }}
            >
              취소
            </button>
            <button
              onClick={config.action}
              style={{
                padding: '10px 20px',
                backgroundColor: getDialogColor(config.type),
                color: 'white',
                border: 'none',
                borderRadius: '6px',
                fontSize: '14px',
                fontWeight: '500',
                cursor: 'pointer'
              }}
            >
              {config.confirmText}
            </button>
          </div>
        </div>
      </div>
    );
  };

  // 성공 메시지 컴포넌트
  const SuccessMessage: React.FC<{ message: string }> = ({ message }) => (
    <div style={{
      position: 'fixed',
      top: '20px',
      right: '20px',
      backgroundColor: '#dcfce7',
      border: '1px solid #16a34a',
      borderRadius: '8px',
      padding: '12px 16px',
      color: '#166534',
      fontSize: '14px',
      fontWeight: '500',
      zIndex: 9999,
      boxShadow: '0 4px 6px rgba(0, 0, 0, 0.1)',
      display: 'flex',
      alignItems: 'center',
      gap: '8px'
    }}>
      ✅ {message}
    </div>
  );

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
      width: '100%',
      maxWidth: 'none',
      padding: '24px',
      backgroundColor: '#f8fafc'
    }}>
      {/* 성공 메시지 */}
      {successMessage && <SuccessMessage message={successMessage} />}

      {/* 확인 다이얼로그 */}
      <ConfirmDialog config={confirmModal} />

      {/* 운영 대시보드 헤더 */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '32px'
      }}>
        <div>
          <h1 style={{
            fontSize: '32px',
            fontWeight: '800',
            color: '#0f172a',
            margin: 0,
            marginBottom: '4px',
            letterSpacing: '-0.025em'
          }}>
            운영 대시보드
          </h1>
          <div style={{
            fontSize: '15px',
            color: '#64748b',
            display: 'flex',
            alignItems: 'center',
            gap: '12px'
          }}>
            <span>PulseOne 실시간 가동 및 데이터 모니터링</span>
            <span style={{
              padding: '4px 10px',
              borderRadius: '20px',
              background: connectionStatus === 'connected' ? '#ecfdf5' : '#fef2f2',
              color: connectionStatus === 'connected' ? '#059669' : '#dc2626',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontSize: '12px',
              fontWeight: '600'
            }}>
              <span style={{
                width: '8px',
                height: '8px',
                borderRadius: '50%',
                background: connectionStatus === 'connected' ? '#10b981' : '#ef4444',
                boxShadow: connectionStatus === 'connected' ? '0 0 8px #10b981' : 'none'
              }}></span>
              {connectionStatus === 'connected' ? '서버 연결됨' :
                connectionStatus === 'reconnecting' ? '재연결 중' : '서버 연결 끊김'}
            </span>
          </div>
        </div>
        <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
          <div style={{
            fontSize: '13px',
            color: '#94a3b8',
            marginRight: '12px',
            textAlign: 'right'
          }}>
            최근 업데이트: {new Date(dashboardData?.last_updated || Date.now()).toLocaleTimeString()}
          </div>
          <button
            onClick={() => setAutoRefresh(!autoRefresh)}
            style={{
              padding: '10px 18px',
              background: autoRefresh ? '#f1f5f9' : '#3b82f6',
              color: autoRefresh ? '#475569' : 'white',
              border: '1px solid #e2e8f0',
              borderRadius: '10px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              fontSize: '14px',
              fontWeight: '600',
              transition: 'all 0.2s'
            }}
          >
            {autoRefresh ? '⏸ 일시정지' : '▶️ 재개'}
          </button>
          <button
            onClick={handleRefreshConfirm}
            style={{
              padding: '10px 18px',
              background: '#0f172a',
              color: 'white',
              border: 'none',
              borderRadius: '10px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              fontSize: '14px',
              fontWeight: '600',
              transition: 'all 0.2s'
            }}
          >
            🔄 즉시 갱신
          </button>
        </div>
      </div>

      {/* 4대 요약 지표 */}
      {dashboardData && (
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(4, 1fr)',
          gap: '24px',
          marginBottom: '32px'
        }}>
          <SummaryTile
            title="업스트림 연결성"
            value={`${dashboardData?.device_summary?.connected_devices || 0}/${dashboardData?.device_summary?.total_devices || 0}`}
            subValue={`연결률: ${dashboardData?.communication_status?.upstream?.connectivity_rate || 0}%`}
            icon="🔌"
            color="#3b82f6"
            trend={{ value: 0, label: '안정적', positive: true }}
          />
          <SummaryTile
            title="다운스트림 내보내기"
            value={`${dashboardData?.communication_status?.downstream?.success_rate || 0}`}
            unit="%"
            subValue={`총 전송수: ${dashboardData?.communication_status?.downstream?.total_exports || 0}`}
            icon="📤"
            color="#10b981"
            trend={{ value: 2.1, label: '상승중', positive: true }}
          />
          <SummaryTile
            title="실시간 활성 알람"
            value={dashboardData?.alarms?.active_total || 0}
            subValue={`오늘 발생: ${dashboardData?.alarms?.today_total || 0}`}
            icon="🔔"
            color="#ef4444"
            trend={{ value: dashboardData?.alarms?.critical || 0, label: `심각 알람: ${dashboardData?.alarms?.critical || 0}`, positive: false }}
          />
          <SummaryTile
            title="시스템 건전성"
            value={dashboardData?.health_status?.overall === 'healthy' ? '정상' : dashboardData?.health_status?.overall === 'degraded' ? '주의' : '장애'}
            subValue={`CPU: ${dashboardData?.system_metrics?.cpuUsage || 0}% | MEM: ${dashboardData?.system_metrics?.memoryUsage || 0}%`}
            icon="🛡️"
            color={dashboardData?.health_status?.overall === 'healthy' ? '#10b981' : dashboardData?.health_status?.overall === 'degraded' ? '#f59e0b' : '#ef4444'}
          />
        </div>
      )}

      {/* 에러 표시 */}
      {error && (
        <div style={{
          background: '#fef2f2',
          border: '1px solid #fecaca',
          borderRadius: '12px',
          padding: '20px',
          marginBottom: '32px',
          display: 'flex',
          alignItems: 'center',
          gap: '16px',
          boxShadow: '0 4px 12px rgba(239, 68, 68, 0.1)'
        }}>
          <span style={{ fontSize: '24px' }}>⚠️</span>
          <div>
            <div style={{ fontWeight: '700', color: '#991b1b', fontSize: '16px' }}>네트워크 연결 오류</div>
            <div style={{ color: '#dc2626', fontSize: '14px' }}>{error}</div>
          </div>
        </div>
      )}

      {/* 📊 메인 레이아웃: 운영 인사이트 (장치 상태 및 알람 트렌드) */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'minmax(400px, 1fr) 1.5fr',
        gap: '24px',
        alignItems: 'start',
        marginBottom: '24px'
      }}>
        {/* 📊 장치 상태 분포 (Device Distribution) */}
        {dashboardData && (
          <div style={{
            background: 'white',
            borderRadius: '20px',
            boxShadow: '0 4px 25px rgba(0, 0, 0, 0.04)',
            border: '1px solid #f1f5f9',
            padding: '28px'
          }}>
            <div style={{ marginBottom: '24px' }}>
              <div style={{ fontSize: '18px', fontWeight: '800', color: '#1e293b' }}>장치 연결 상태 분포</div>
              <div style={{ fontSize: '13px', color: '#64748b', marginTop: '2px' }}>현장에 설치된 전체 장치 가동 현황</div>
            </div>

            <div style={{ display: 'flex', alignItems: 'center', gap: '32px' }}>
              {/* 시각화 (Simple Bar Chart instead of complex Pie) */}
              <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '16px' }}>
                {[
                  { label: '정상 연결', count: dashboardData?.device_summary?.connected_devices || 0, color: '#10b981', total: dashboardData?.device_summary?.total_devices || 0 },
                  { label: '연결 끊김', count: dashboardData?.device_summary?.disconnected_devices || 0, color: '#64748b', total: dashboardData?.device_summary?.total_devices || 0 },
                  { label: '통신 오류', count: dashboardData?.device_summary?.error_devices || 0, color: '#ef4444', total: dashboardData?.device_summary?.total_devices || 0 }
                ].map((item) => (
                  <div key={item.label}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', fontSize: '13px' }}>
                      <span style={{ fontWeight: '600', color: '#475569' }}>{item.label}</span>
                      <span style={{ fontWeight: '700', color: '#1e293b' }}>{item.count}대 ({item.total > 0 ? Math.round((item.count / item.total) * 100) : 0}%)</span>
                    </div>
                    <div style={{ width: '100%', height: '8px', background: '#f1f5f9', borderRadius: '4px', overflow: 'hidden' }}>
                      <div style={{
                        width: `${item.total > 0 ? (item.count / item.total) * 100 : 0}%`,
                        height: '100%',
                        background: item.color,
                        borderRadius: '4px',
                        transition: 'width 1s ease-out'
                      }}></div>
                    </div>
                  </div>
                ))}
              </div>
            </div>

            <div style={{
              marginTop: '28px',
              padding: '16px',
              background: '#f8fafc',
              borderRadius: '12px',
              border: '1px solid #f1f5f9'
            }}>
              <div style={{ fontSize: '13px', fontWeight: '700', color: '#475569', marginBottom: '8px' }}>프로토콜별 장치 현황</div>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
                {(dashboardData.device_summary.protocol_details || []).map(p => (
                  <div key={p.type} style={{
                    padding: '6px 12px',
                    background: 'white',
                    border: '1px solid #e2e8f0',
                    borderRadius: '8px',
                    fontSize: '12px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px'
                  }}>
                    <span style={{ fontWeight: '700', color: '#334155' }}>{p.type}</span>
                    <span style={{ color: '#94a3b8' }}>|</span>
                    <span style={{ fontWeight: '600', color: '#3b82f6' }}>{p.count}대</span>
                  </div>
                ))}
                {(!dashboardData.device_summary.protocol_details || dashboardData.device_summary.protocol_details.length === 0) && (
                  <div style={{ fontSize: '12px', color: '#94a3b8' }}>등록된 장치가 없습니다.</div>
                )}
              </div>
            </div>
          </div>
        )}

        {/* 🔔 알람 핫스팟 및 트렌드 (Alarm Hotspots) */}
        {dashboardData && (
          <div style={{
            background: 'white',
            borderRadius: '20px',
            boxShadow: '0 4px 25px rgba(0, 0, 0, 0.04)',
            border: '1px solid #f1f5f9',
            display: 'flex',
            flexDirection: 'column'
          }}>
            <div style={{
              padding: '24px 28px',
              borderBottom: '1px solid #f1f5f9',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <div>
                <div style={{ fontSize: '18px', fontWeight: '800', color: '#1e293b' }}>실시간 운영 현황 및 알람</div>
                <div style={{ fontSize: '13px', color: '#64748b', marginTop: '2px' }}>현장별 실시간 상태 및 최근 주요 이벤트</div>
              </div>
              <button
                onClick={() => window.location.href = '/alarms'}
                style={{ color: '#3b82f6', background: 'none', border: 'none', fontSize: '14px', fontWeight: '700', cursor: 'pointer', padding: '8px', borderRadius: '8px', transition: 'background 0.2s' }}
                onMouseOver={(e) => (e.currentTarget.style.background = '#eff6ff')}
                onMouseOut={(e) => (e.currentTarget.style.background = 'none')}
              >
                상세 이력 보기 →
              </button>
            </div>

            <div style={{ padding: '24px 28px', display: 'flex', gap: '24px', flex: 1 }}>
              {/* 현장별 상태 (Hotspots) */}
              <div style={{ flex: 1 }}>
                <div style={{ fontSize: '13px', fontWeight: '700', color: '#475569', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <span style={{ width: '3px', height: '12px', background: '#3b82f6', borderRadius: '2px' }}></span>
                  현장별 가동 상태
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                  {(dashboardData.device_summary as any).protocol_details?.map((proto: any) => (
                    <div key={proto.type} style={{
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      padding: '12px 16px',
                      background: '#f8fafc',
                      borderRadius: '12px',
                      border: '1px solid #f1f5f9'
                    }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                        <span style={{ fontSize: '18px' }}>{proto.type === 'modbus' ? '🧩' : '🔌'}</span>
                        <div>
                          <div style={{ fontSize: '14px', fontWeight: '700', color: '#1e293b' }}>{proto.name}</div>
                          <div style={{ fontSize: '12px', color: '#64748b' }}>{proto.type.toUpperCase()}</div>
                        </div>
                      </div>
                      <div style={{ textAlign: 'right' }}>
                        <div style={{ fontSize: '16px', fontWeight: '800', color: '#1e293b' }}>{proto.count}</div>
                        <div style={{ fontSize: '11px', color: '#10b981', fontWeight: '600' }}>{proto.connected} Connected</div>
                      </div>
                    </div>
                  ))}    </div>
              </div>

              {/* 최근 이벤트 피드 */}
              <div style={{ flex: 1.2 }}>
                <div style={{ fontSize: '13px', fontWeight: '700', color: '#475569', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <span style={{ width: '3px', height: '12px', background: '#ef4444', borderRadius: '2px' }}></span>
                  최근 주요 이벤트
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                  {(dashboardData.alarms.recent_alarms || []).length > 0 ? (
                    dashboardData.alarms.recent_alarms.slice(0, 4).map((alarm) => (
                      <div key={alarm.id} style={{
                        padding: '12px',
                        borderRadius: '12px',
                        background: alarm.severity === 'critical' ? '#fff1f2' : (alarm.severity === 'major' ? '#fffbeb' : '#f8fafc'),
                        border: `1px solid ${alarm.severity === 'critical' ? '#fecaca' : (alarm.severity === 'major' ? '#fde68a' : '#f1f5f9')}`,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '12px'
                      }}>
                        <div style={{ fontSize: '18px' }}>
                          {alarm.severity === 'critical' ? '🚨' : (alarm.severity === 'major' ? '🔶' : '🔔')}
                        </div>
                        <div style={{ flex: 1, minWidth: 0 }}>
                          <div style={{ fontSize: '13px', fontWeight: '700', color: '#1e293b', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{alarm.message}</div>
                          <div style={{ fontSize: '11px', color: '#64748b', marginTop: '1px' }}>
                            {alarm.device_name || '시스템'} • {new Date(alarm.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
                          </div>
                        </div>
                      </div>
                    ))
                  ) : (
                    <div style={{ padding: '32px', textAlign: 'center', background: '#f8fafc', borderRadius: '12px', border: '1px dashed #e2e8f0' }}>
                      <div style={{ fontSize: '13px', color: '#94a3b8' }}>최근 알람이 없습니다.</div>
                    </div>
                  )}
                </div>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* 📊 하단: 성능 지표 및 자산 요약 */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(2, 1fr)',
        gap: '24px',
        marginBottom: '40px'
      }}>
        {dashboardData && (
          <>
            <div style={{
              background: 'white',
              borderRadius: '20px',
              padding: '24px 28px',
              border: '1px solid #f1f5f9',
              boxShadow: '0 4px 20px rgba(0, 0, 0, 0.04)'
            }}>
              <div style={{ fontSize: '14px', fontWeight: '800', color: '#1e293b', marginBottom: '20px', textTransform: 'uppercase', letterSpacing: '0.05em', display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ fontSize: '18px' }}>⚡</span> 시스템 성능 지수
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '24px' }}>
                <div style={{ textAlign: 'center', padding: '16px', background: '#f8fafc', borderRadius: '16px' }}>
                  <div style={{ color: '#64748b', fontSize: '12px', fontWeight: '600', marginBottom: '8px' }}>API 응답속도</div>
                  <div style={{ fontWeight: '800', color: '#1e293b', fontSize: '20px' }}>{dashboardData.performance.api_response_time}<span style={{ fontSize: '12px', color: '#94a3b8', marginLeft: '2px' }}>ms</span></div>
                </div>
                <div style={{ textAlign: 'center', padding: '16px', background: '#f8fafc', borderRadius: '16px' }}>
                  <div style={{ color: '#64748b', fontSize: '12px', fontWeight: '600', marginBottom: '8px' }}>DB 쿼리 시간</div>
                  <div style={{ fontWeight: '800', color: '#1e293b', fontSize: '20px' }}>{dashboardData.performance.database_response_time}<span style={{ fontSize: '12px', color: '#94a3b8', marginLeft: '2px' }}>ms</span></div>
                </div>
                <div style={{ textAlign: 'center', padding: '16px', background: '#f8fafc', borderRadius: '16px' }}>
                  <div style={{ color: '#64748b', fontSize: '12px', fontWeight: '600', marginBottom: '8px' }}>데이터 처리량</div>
                  <div style={{ fontWeight: '800', color: '#1e293b', fontSize: '20px', whiteSpace: 'nowrap' }}>
                    {dashboardData.performance.throughput_per_second}<span style={{ fontSize: '12px', color: '#94a3b8', marginLeft: '2px' }}>req/s</span>
                  </div>
                </div>
              </div>
            </div>

            <div style={{
              background: 'white',
              borderRadius: '20px',
              padding: '24px 28px',
              border: '1px solid #f1f5f9',
              boxShadow: '0 4px 20px rgba(0, 0, 0, 0.04)'
            }}>
              <div style={{ fontSize: '14px', fontWeight: '800', color: '#1e293b', marginBottom: '20px', textTransform: 'uppercase', letterSpacing: '0.05em', display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ fontSize: '18px' }}>📊</span> 통합 자산 요약
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '24px' }}>
                <div style={{ textAlign: 'center', padding: '16px', background: '#f0f9ff', borderRadius: '16px', border: '1px solid #e0f2fe' }}>
                  <div style={{ color: '#0369a1', fontSize: '12px', fontWeight: '600', marginBottom: '8px' }}>등록 프로토콜</div>
                  <div style={{ fontWeight: '800', color: '#0c4a6e', fontSize: '20px' }}>{dashboardData?.device_summary?.protocol_details?.length || 0}<span style={{ fontSize: '12px', marginLeft: '2px' }}>종</span></div>
                </div>
                <div style={{ textAlign: 'center', padding: '16px', background: '#ecfdf5', borderRadius: '16px', border: '1px solid #d1fae5' }}>
                  <div style={{ color: '#047857', fontSize: '12px', fontWeight: '600', marginBottom: '8px' }}>활성 현장 (Site)</div>
                  <div style={{ fontWeight: '800', color: '#064e3b', fontSize: '20px' }}>{dashboardData?.device_summary?.sites_count || 0}<span style={{ fontSize: '12px', marginLeft: '2px' }}>개소</span></div>
                </div>
                <div style={{ textAlign: 'center', padding: '16px', background: '#fffbeb', borderRadius: '16px', border: '1px solid #fef3c7' }}>
                  <div style={{ color: '#b45309', fontSize: '12px', fontWeight: '600', marginBottom: '8px' }}>수집 포인트 수</div>
                  <div style={{ fontWeight: '800', color: '#78350f', fontSize: '20px' }}>
                    {((dashboardData?.device_summary?.data_points_count || 0) / 1000).toFixed(1)}<span style={{ fontSize: '12px', marginLeft: '2px' }}>k EA</span>
                  </div>
                </div>
              </div>
            </div>
          </>
        )}
      </div>

      {/* Flow Monitor (가장 하단으로 이동) */}
      <FlowMonitor data={dashboardData} />

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