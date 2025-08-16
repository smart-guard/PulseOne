// ============================================================================
// frontend/src/pages/Dashboard.tsx
// 📝 대시보드 메인 페이지 - 새로운 통합 API 완전 연결
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { ApiService } from '../api';
import '../styles/base.css';
import '../styles/dashboard.css';

// 🎯 대시보드 관련 인터페이스들
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
  last_updated: string;
}

interface ServiceInfo {
  name: string;
  displayName: string;
  status: 'running' | 'stopped' | 'error';
  icon: string;
  controllable: boolean;
  description: string;
  uptime?: number;
  memory_usage?: number;
  cpu_usage?: number;
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
}

interface DeviceSummary {
  total_devices: number;
  connected_devices: number;
  disconnected_devices: number;
  error_devices: number;
  protocols_count: number;
  sites_count: number;
}

interface AlarmSummary {
  total: number;
  unacknowledged: number;
  critical: number;
  warnings: number;
  recent_alarms: RecentAlarm[];
}

interface RecentAlarm {
  id: string;
  type: 'error' | 'warning' | 'info';
  message: string;
  icon: string;
  timestamp: string;
  device_id: number;
  acknowledged: boolean;
}

interface HealthStatus {
  overall: 'healthy' | 'degraded' | 'critical';
  database: 'healthy' | 'warning' | 'critical';
  network: 'healthy' | 'warning' | 'critical';
  storage: 'healthy' | 'warning' | 'critical';
}

const Dashboard: React.FC = () => {
  // 🔧 기본 상태들
  const [dashboardData, setDashboardData] = useState<DashboardData | null>(null);
  const [realtimeStats, setRealtimeStats] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  
  // 실시간 업데이트
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(10000); // 10초
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  // 차트 데이터 (시뮬레이션)
  const [chartData, setChartData] = useState<{
    dataPoints: number[];
    timestamps: string[];
    systemLoad: number[];
  }>({
    dataPoints: [],
    timestamps: [],
    systemLoad: []
  });

  // =============================================================================
  // 🔄 데이터 로드 함수들 (새로운 통합 API 사용)
  // =============================================================================

  /**
   * 대시보드 개요 데이터 로드
   */
  const loadDashboardOverview = useCallback(async () => {
    try {
      setIsLoading(true);
      setError(null);

      console.log('🎯 대시보드 개요 데이터 로드 시작...');

      // 새로운 대시보드 API 호출
      const response = await fetch('/api/dashboard/overview');
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();

      if (data.success && data.data) {
        setDashboardData(data.data);
        console.log('✅ 대시보드 개요 데이터 로드 완료');
      } else {
        throw new Error(data.message || '대시보드 데이터 로드 실패');
      }

    } catch (err) {
      console.error('❌ 대시보드 데이터 로드 실패:', err);
      setError(err instanceof Error ? err.message : '알 수 없는 오류');
    } finally {
      setIsLoading(false);
      setLastUpdate(new Date());
    }
  }, []);

  /**
   * 실시간 통계 로드
   */
  const loadRealtimeStats = useCallback(async () => {
    try {
      console.log('⚡ 실시간 통계 로드 시작...');

      const response = await ApiService.realtime.getRealtimeStats();

      if (response.success && response.data) {
        setRealtimeStats(response.data);
        console.log('✅ 실시간 통계 로드 완료');
      } else {
        console.warn('⚠️ 실시간 통계 로드 실패:', response.error);
      }
    } catch (err) {
      console.warn('⚠️ 실시간 통계 로드 실패:', err);
    }
  }, []);

  /**
   * 시스템 전체 개요 로드 (통합 API 사용)
   */
  const loadSystemOverview = useCallback(async () => {
    try {
      console.log('🔄 시스템 전체 개요 로드 시작...');

      const overview = await ApiService.getSystemOverview();
      
      // 차트 데이터 업데이트
      const now = new Date();
      const timeLabel = now.toLocaleTimeString();
      
      setChartData(prev => ({
        dataPoints: [...prev.dataPoints.slice(-19), overview.realtime?.performance?.data_points_monitored || 0],
        timestamps: [...prev.timestamps.slice(-19), timeLabel],
        systemLoad: [...prev.systemLoad.slice(-19), overview.system?.overall === 'healthy' ? 85 : 60]
      }));

      console.log('✅ 시스템 전체 개요 로드 완료');
    } catch (err) {
      console.warn('⚠️ 시스템 전체 개요 로드 실패:', err);
    }
  }, []);

  // =============================================================================
  // 🔄 서비스 제어 함수들
  // =============================================================================

  /**
   * 서비스 제어 (시작/중지/재시작)
   */
  const handleServiceControl = async (serviceName: string, action: 'start' | 'stop' | 'restart') => {
    try {
      console.log(`🔧 서비스 ${serviceName} ${action} 요청...`);

      const response = await fetch(`/api/dashboard/service/${serviceName}/control`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({ action })
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();

      if (data.success) {
        console.log(`✅ 서비스 ${serviceName} ${action} 완료`);
        alert(`서비스 ${action} 완료: ${data.data.message}`);
        
        // 서비스 상태 새로고침
        setTimeout(() => {
          loadDashboardOverview();
        }, 2000); // 2초 후 새로고침

      } else {
        throw new Error(data.message || `서비스 ${action} 실패`);
      }
    } catch (err) {
      console.error(`❌ 서비스 ${serviceName} ${action} 실패:`, err);
      alert(`서비스 ${action} 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    }
  };

  // =============================================================================
  // 🔄 라이프사이클 hooks
  // =============================================================================

  useEffect(() => {
    loadDashboardOverview();
    loadRealtimeStats();
    loadSystemOverview();
  }, [loadDashboardOverview, loadRealtimeStats, loadSystemOverview]);

  // 자동 새로고침
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      loadDashboardOverview();
      loadRealtimeStats();
      loadSystemOverview();
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, loadDashboardOverview, loadRealtimeStats, loadSystemOverview]);

  // =============================================================================
  // 🎨 렌더링 헬퍼 함수들
  // =============================================================================

  const getServiceStatusIcon = (status: string) => {
    switch (status) {
      case 'running': return 'fas fa-check-circle text-success';
      case 'stopped': return 'fas fa-stop-circle text-warning';
      case 'error': return 'fas fa-times-circle text-danger';
      default: return 'fas fa-question-circle text-muted';
    }
  };

  const getHealthStatusColor = (status: string) => {
    switch (status) {
      case 'healthy': return 'success';
      case 'degraded': 
      case 'warning': return 'warning';
      case 'critical': return 'danger';
      default: return 'muted';
    }
  };

  const getAlarmTypeIcon = (type: string) => {
    switch (type) {
      case 'error': return 'fas fa-exclamation-triangle text-danger';
      case 'warning': return 'fas fa-exclamation-circle text-warning';
      case 'info': return 'fas fa-info-circle text-info';
      default: return 'fas fa-bell text-muted';
    }
  };

  const formatUptime = (seconds?: number) => {
    if (!seconds) return '알 수 없음';
    
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

  const formatMemoryUsage = (mb?: number) => {
    if (!mb) return '알 수 없음';
    
    if (mb >= 1024) {
      return `${(mb / 1024).toFixed(1)}GB`;
    } else {
      return `${mb}MB`;
    }
  };

  // =============================================================================
  // 🎨 UI 렌더링
  // =============================================================================

  if (isLoading && !dashboardData) {
    return (
      <div className="dashboard-loading">
        <div className="loading-spinner">
          <i className="fas fa-spinner fa-spin"></i>
          <span>대시보드를 불러오는 중...</span>
        </div>
      </div>
    );
  }

  if (error && !dashboardData) {
    return (
      <div className="dashboard-error">
        <div className="error-message">
          <i className="fas fa-exclamation-triangle"></i>
          <h3>대시보드 로드 실패</h3>
          <p>{error}</p>
          <button 
            className="btn btn-primary"
            onClick={() => {
              setError(null);
              loadDashboardOverview();
            }}
          >
            <i className="fas fa-redo"></i>
            다시 시도
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="dashboard-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-left">
          <h1 className="page-title">시스템 대시보드</h1>
          <div className="page-subtitle">
            PulseOne 시스템의 전체 현황을 실시간으로 모니터링합니다
          </div>
        </div>
        <div className="header-right">
          <div className="header-actions">
            <button 
              className="btn btn-secondary"
              onClick={() => setAutoRefresh(!autoRefresh)}
            >
              <i className={`fas fa-${autoRefresh ? 'pause' : 'play'}`}></i>
              {autoRefresh ? '자동새로고침 중지' : '자동새로고침 시작'}
            </button>
            <button 
              className="btn btn-primary"
              onClick={() => {
                loadDashboardOverview();
                loadRealtimeStats();
                loadSystemOverview();
              }}
            >
              <i className="fas fa-sync-alt"></i>
              새로고침
            </button>
          </div>
        </div>
      </div>

      {/* 에러 표시 */}
      {error && (
        <div className="error-banner">
          <i className="fas fa-exclamation-triangle"></i>
          {error}
          <button onClick={() => setError(null)}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 시스템 상태 개요 */}
      {dashboardData && (
        <div className="system-overview">
          <div className="overview-card">
            <div className="card-header">
              <h3>시스템 상태</h3>
              <span className={`health-badge health-${dashboardData.health_status.overall}`}>
                <i className="fas fa-heartbeat"></i>
                {dashboardData.health_status.overall === 'healthy' ? '정상' : 
                 dashboardData.health_status.overall === 'degraded' ? '주의' : '심각'}
              </span>
            </div>
            <div className="card-body">
              <div className="health-details">
                <div className="health-item">
                  <span className="label">데이터베이스:</span>
                  <span className={`value text-${getHealthStatusColor(dashboardData.health_status.database)}`}>
                    {dashboardData.health_status.database === 'healthy' ? '정상' : '문제'}
                  </span>
                </div>
                <div className="health-item">
                  <span className="label">네트워크:</span>
                  <span className={`value text-${getHealthStatusColor(dashboardData.health_status.network)}`}>
                    {dashboardData.health_status.network === 'healthy' ? '정상' : '문제'}
                  </span>
                </div>
                <div className="health-item">
                  <span className="label">저장소:</span>
                  <span className={`value text-${getHealthStatusColor(dashboardData.health_status.storage)}`}>
                    {dashboardData.health_status.storage === 'healthy' ? '정상' : '문제'}
                  </span>
                </div>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* 주요 메트릭 카드들 */}
      {dashboardData && (
        <div className="metrics-grid">
          <div className="metric-card">
            <div className="metric-icon">
              <i className="fas fa-network-wired text-primary"></i>
            </div>
            <div className="metric-content">
              <div className="metric-value">{dashboardData.device_summary.total_devices}</div>
              <div className="metric-label">전체 디바이스</div>
              <div className="metric-detail">
                연결: {dashboardData.device_summary.connected_devices} / 
                끊김: {dashboardData.device_summary.disconnected_devices}
              </div>
            </div>
          </div>

          <div className="metric-card">
            <div className="metric-icon">
              <i className="fas fa-chart-line text-success"></i>
            </div>
            <div className="metric-content">
              <div className="metric-value">{dashboardData.system_metrics.dataPointsPerSecond}</div>
              <div className="metric-label">데이터 포인트/초</div>
              <div className="metric-detail">
                평균 응답시간: {dashboardData.system_metrics.avgResponseTime}ms
              </div>
            </div>
          </div>

          <div className="metric-card">
            <div className="metric-icon">
              <i className="fas fa-bell text-warning"></i>
            </div>
            <div className="metric-content">
              <div className="metric-value">{dashboardData.alarms.total}</div>
              <div className="metric-label">활성 알람</div>
              <div className="metric-detail">
                심각: {dashboardData.alarms.critical} / 
                미확인: {dashboardData.alarms.unacknowledged}
              </div>
            </div>
          </div>

          <div className="metric-card">
            <div className="metric-icon">
              <i className="fas fa-server text-info"></i>
            </div>
            <div className="metric-content">
              <div className="metric-value">{dashboardData.services.running}</div>
              <div className="metric-label">실행 중인 서비스</div>
              <div className="metric-detail">
                전체: {dashboardData.services.total} / 
                중지: {dashboardData.services.stopped}
              </div>
            </div>
          </div>
        </div>
      )}

      {/* 콘텐츠 그리드 */}
      <div className="dashboard-content">
        {/* 서비스 상태 */}
        {dashboardData && (
          <div className="dashboard-section">
            <div className="section-header">
              <h3>서비스 상태</h3>
              <div className="section-actions">
                <span className="service-summary">
                  실행중: {dashboardData.services.running} / 
                  전체: {dashboardData.services.total}
                </span>
              </div>
            </div>
            <div className="services-list">
              {dashboardData.services.details.map((service) => (
                <div key={service.name} className="service-item">
                  <div className="service-info">
                    <div className="service-header">
                      <i className={service.icon}></i>
                      <span className="service-name">{service.displayName}</span>
                      <span className={`service-status status-${service.status}`}>
                        <i className={getServiceStatusIcon(service.status)}></i>
                        {service.status === 'running' ? '실행중' : 
                         service.status === 'stopped' ? '중지됨' : '오류'}
                      </span>
                    </div>
                    <div className="service-description">{service.description}</div>
                    <div className="service-metrics">
                      {service.uptime && (
                        <span>업타임: {formatUptime(service.uptime)}</span>
                      )}
                      {service.memory_usage && (
                        <span>메모리: {formatMemoryUsage(service.memory_usage)}</span>
                      )}
                      {service.cpu_usage && (
                        <span>CPU: {service.cpu_usage}%</span>
                      )}
                    </div>
                  </div>
                  {service.controllable && (
                    <div className="service-actions">
                      {service.status === 'running' ? (
                        <>
                          <button 
                            onClick={() => handleServiceControl(service.name, 'stop')}
                            className="btn btn-sm btn-warning"
                            title="중지"
                          >
                            <i className="fas fa-stop"></i>
                          </button>
                          <button 
                            onClick={() => handleServiceControl(service.name, 'restart')}
                            className="btn btn-sm btn-secondary"
                            title="재시작"
                          >
                            <i className="fas fa-redo"></i>
                          </button>
                        </>
                      ) : (
                        <button 
                          onClick={() => handleServiceControl(service.name, 'start')}
                          className="btn btn-sm btn-success"
                          title="시작"
                        >
                          <i className="fas fa-play"></i>
                        </button>
                      )}
                    </div>
                  )}
                </div>
              ))}
            </div>
          </div>
        )}

        {/* 시스템 리소스 */}
        {dashboardData && (
          <div className="dashboard-section">
            <div className="section-header">
              <h3>시스템 리소스</h3>
            </div>
            <div className="resource-meters">
              <div className="resource-meter">
                <div className="meter-header">
                  <span className="meter-label">CPU 사용률</span>
                  <span className="meter-value">{dashboardData.system_metrics.cpuUsage}%</span>
                </div>
                <div className="meter-bar">
                  <div 
                    className="meter-fill"
                    style={{ width: `${dashboardData.system_metrics.cpuUsage}%` }}
                  ></div>
                </div>
              </div>

              <div className="resource-meter">
                <div className="meter-header">
                  <span className="meter-label">메모리 사용률</span>
                  <span className="meter-value">{dashboardData.system_metrics.memoryUsage}%</span>
                </div>
                <div className="meter-bar">
                  <div 
                    className="meter-fill"
                    style={{ width: `${dashboardData.system_metrics.memoryUsage}%` }}
                  ></div>
                </div>
              </div>

              <div className="resource-meter">
                <div className="meter-header">
                  <span className="meter-label">디스크 사용률</span>
                  <span className="meter-value">{dashboardData.system_metrics.diskUsage}%</span>
                </div>
                <div className="meter-bar">
                  <div 
                    className="meter-fill"
                    style={{ width: `${dashboardData.system_metrics.diskUsage}%` }}
                  ></div>
                </div>
              </div>

              <div className="resource-meter">
                <div className="meter-header">
                  <span className="meter-label">네트워크 사용률</span>
                  <span className="meter-value">{dashboardData.system_metrics.networkUsage} Mbps</span>
                </div>
                <div className="meter-bar">
                  <div 
                    className="meter-fill"
                    style={{ width: `${Math.min(dashboardData.system_metrics.networkUsage, 100)}%` }}
                  ></div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* 최근 알람 */}
        {dashboardData && dashboardData.alarms.recent_alarms.length > 0 && (
          <div className="dashboard-section">
            <div className="section-header">
              <h3>최근 알람</h3>
              <a href="#/alarms/active" className="section-link">
                모든 알람 보기 <i className="fas fa-arrow-right"></i>
              </a>
            </div>
            <div className="recent-alarms">
              {dashboardData.alarms.recent_alarms.map((alarm) => (
                <div key={alarm.id} className="alarm-item">
                  <div className="alarm-icon">
                    <i className={getAlarmTypeIcon(alarm.type)}></i>
                  </div>
                  <div className="alarm-content">
                    <div className="alarm-message">{alarm.message}</div>
                    <div className="alarm-meta">
                      <span className="alarm-time">
                        {new Date(alarm.timestamp).toLocaleString()}
                      </span>
                      <span className="alarm-device">
                        Device {alarm.device_id}
                      </span>
                      {alarm.acknowledged && (
                        <span className="alarm-ack">
                          <i className="fas fa-check"></i> 확인됨
                        </span>
                      )}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* 실시간 통계 */}
        {realtimeStats && (
          <div className="dashboard-section">
            <div className="section-header">
              <h3>실시간 데이터</h3>
            </div>
            <div className="realtime-stats">
              <div className="stat-item">
                <div className="stat-icon">
                  <i className="fas fa-broadcast-tower text-primary"></i>
                </div>
                <div className="stat-content">
                  <div className="stat-value">{realtimeStats.realtime_connections}</div>
                  <div className="stat-label">실시간 연결</div>
                </div>
              </div>
              <div className="stat-item">
                <div className="stat-icon">
                  <i className="fas fa-chart-bar text-success"></i>
                </div>
                <div className="stat-content">
                  <div className="stat-value">{realtimeStats.messages_per_second}</div>
                  <div className="stat-label">메시지/초</div>
                </div>
              </div>
              <div className="stat-item">
                <div className="stat-icon">
                  <i className="fas fa-database text-info"></i>
                </div>
                <div className="stat-content">
                  <div className="stat-value">{realtimeStats.data_points_monitored}</div>
                  <div className="stat-label">모니터링 포인트</div>
                </div>
              </div>
              <div className="stat-item">
                <div className="stat-icon">
                  <i className="fas fa-clock text-warning"></i>
                </div>
                <div className="stat-content">
                  <div className="stat-value">{realtimeStats.average_latency_ms}ms</div>
                  <div className="stat-label">평균 지연시간</div>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* 상태 정보 */}
      <div className="status-bar">
        <div className="status-info">
          <span>마지막 업데이트: {lastUpdate.toLocaleTimeString()}</span>
          {autoRefresh && (
            <span className="auto-refresh-indicator">
              <i className="fas fa-sync-alt"></i>
              {refreshInterval / 1000}초마다 자동 새로고침
            </span>
          )}
          {dashboardData && (
            <span className="system-status">
              시스템 상태: 
              <span className={`status-badge status-${dashboardData.health_status.overall}`}>
                {dashboardData.health_status.overall === 'healthy' ? '정상' : 
                 dashboardData.health_status.overall === 'degraded' ? '주의' : '심각'}
              </span>
            </span>
          )}
        </div>
      </div>
    </div>
  );
};

export default Dashboard;