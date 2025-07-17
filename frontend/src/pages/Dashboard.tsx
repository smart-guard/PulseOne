
import React, { useState, useEffect } from 'react';
import '../styles/base.css';

// 기존 타입 정의들은 그대로 유지
interface ServiceStatus {
  name: string;
  status: 'running' | 'paused' | 'stopped' | 'error';
  icon: string;
}

interface SystemMetrics {
  dataPointsPerSecond: number;
  avgResponseTime: number;
  dbQueryTime: number;
  cpuUsage: number;
  memoryUsage: number;
  diskUsage: number;
  networkUsage: number;
}

interface TenantStats {
  activeTenants: number;
  edgeServers: number;
  connectedDevices: number;
  uptime: number;
}

interface AlarmItem {
  id: string;
  type: 'error' | 'warning' | 'info';
  message: string;
  timestamp: Date;
  icon: string;
}

interface Device {
  id: string;
  name: string;
  type: string;
  status: 'connected' | 'disconnected' | 'error';
  dataPoints: number;
}

const Dashboard: React.FC = () => {
  // 기존 상태 관리 코드들 그대로 유지
  const [services, setServices] = useState<ServiceStatus[]>([
    { name: 'Database', status: 'running', icon: 'fas fa-database' },
    { name: 'Backend', status: 'running', icon: 'fas fa-code-branch' },
    { name: 'Collector', status: 'running', icon: 'fas fa-download' },
    { name: 'Message Queue', status: 'paused', icon: 'fas fa-exchange-alt' },
  ]);

  const [metrics, setMetrics] = useState<SystemMetrics>({
    dataPointsPerSecond: 2847,
    avgResponseTime: 145,
    dbQueryTime: 23,
    cpuUsage: 34,
    memoryUsage: 67,
    diskUsage: 45,
    networkUsage: 12,
  });

  const [tenantStats, setTenantStats] = useState<TenantStats>({
    activeTenants: 12,
    edgeServers: 24,
    connectedDevices: 156,
    uptime: 98.7,
  });

  const [alarms, setAlarms] = useState<AlarmItem[]>([
    {
      id: '1',
      type: 'error',
      message: 'Factory A - PLC 연결 실패',
      timestamp: new Date(Date.now() - 2 * 60 * 1000),
      icon: 'fas fa-exclamation-circle',
    },
    {
      id: '2',
      type: 'warning',
      message: '메모리 사용률 임계값 초과',
      timestamp: new Date(Date.now() - 5 * 60 * 1000),
      icon: 'fas fa-exclamation-triangle',
    },
    {
      id: '3',
      type: 'info',
      message: '새로운 디바이스 연결됨',
      timestamp: new Date(Date.now() - 8 * 60 * 1000),
      icon: 'fas fa-info-circle',
    },
  ]);

  const [devices, setDevices] = useState<Device[]>([
    { id: '1', name: 'Factory A - PLC #1', type: 'Modbus TCP', status: 'connected', dataPoints: 152 },
    { id: '2', name: 'Factory A - MQTT Broker', type: 'MQTT', status: 'connected', dataPoints: 89 },
    { id: '3', name: 'Factory B - BACnet Gateway', type: 'BACnet', status: 'error', dataPoints: 0 },
    { id: '4', name: 'Factory B - Energy Meter', type: 'Modbus RTU', status: 'connected', dataPoints: 24 },
  ]);

  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [isRefreshing, setIsRefreshing] = useState(false);

  // 기존 useEffect와 함수들 그대로 유지
  useEffect(() => {
    const interval = setInterval(() => {
      setMetrics(prev => ({
        ...prev,
        dataPointsPerSecond: Math.floor(Math.random() * 1000) + 2000,
        cpuUsage: Math.floor(Math.random() * 100),
        avgResponseTime: Math.floor(Math.random() * 100) + 100,
      }));
      setLastUpdate(new Date());
    }, 5000);

    return () => clearInterval(interval);
  }, []);

  const handleRefresh = async () => {
    setIsRefreshing(true);
    await new Promise(resolve => setTimeout(resolve, 1000));
    setLastUpdate(new Date());
    setIsRefreshing(false);
  };

  const getStatusClass = (status: string) => {
    switch (status) {
      case 'running':
      case 'connected':
        return 'status-running';
      case 'paused':
        return 'status-paused';
      case 'stopped':
      case 'disconnected':
        return 'status-stopped';
      case 'error':
        return 'status-error';
      default:
        return 'status-stopped';
    }
  };

  const getStatusDotClass = (status: string) => {
    switch (status) {
      case 'running':
      case 'connected':
        return 'status-dot-running';
      case 'paused':
        return 'status-dot-paused';
      case 'stopped':
      case 'disconnected':
        return 'status-dot-stopped';
      case 'error':
        return 'status-dot-error';
      default:
        return 'status-dot-stopped';
    }
  };

  const getProgressBarClass = (value: number) => {
    if (value < 50) return 'success';
    if (value < 80) return 'warning';
    return 'error';
  };

  const formatTimeAgo = (date: Date) => {
    const now = new Date();
    const diffMs = now.getTime() - date.getTime();
    const diffMins = Math.floor(diffMs / 60000);
    
    if (diffMins < 1) return '방금 전';
    if (diffMins < 60) return `${diffMins}분 전`;
    const diffHours = Math.floor(diffMins / 60);
    if (diffHours < 24) return `${diffHours}시간 전`;
    const diffDays = Math.floor(diffHours / 24);
    return `${diffDays}일 전`;
  };

  return (
    <div className="dashboard-container">
      {/* 대시보드 헤더 */}
      <div className="dashboard-header">
        <h1 className="dashboard-title">시스템 모니터링 대시보드</h1>
        <div className="dashboard-actions">
          <select className="form-select">
            <option>전체 시스템</option>
            <option>Factory A</option>
            <option>Factory B</option>
          </select>
        </div>
      </div>

      {/* 상태 표시줄 */}
      <div className="dashboard-status-bar">
        <div className="connection-indicator">
          <div className="live-indicator"></div>
          실시간 연결됨
        </div>
        <div className="dashboard-controls">
          <span className="text-sm text-neutral-600">
            마지막 업데이트: {formatTimeAgo(lastUpdate)}
          </span>
          <button className="refresh-btn" onClick={handleRefresh} disabled={isRefreshing}>
            <i className={`fas ${isRefreshing ? 'fa-spinner fa-spin' : 'fa-sync-alt'}`}></i>
            {isRefreshing ? '업데이트 중...' : '새로고침'}
          </button>
        </div>
      </div>

      {/* 대시보드 그리드 */}
      <div className="dashboard-grid">
        {/* 서비스 상태 */}
        <div className="dashboard-widget">
          <div className="widget-header">
            <div className="widget-title">
              <i className="fas fa-server text-primary-600"></i>
              서비스 상태
            </div>
            <span className="status status-running">
              <div className="status-dot status-dot-running"></div>
              모든 서비스 정상
            </span>
          </div>
          <div className="widget-content">
            <div className="service-grid">
              {services.map((service) => (
                <div key={service.name} className="service-card">
                  <div className={`service-icon ${getStatusClass(service.status) === 'status-running' ? 'text-success-600' : 
                    getStatusClass(service.status) === 'status-paused' ? 'text-warning-600' : 'text-error-600'}`}>
                    <i className={service.icon}></i>
                  </div>
                  <div className="service-name">{service.name}</div>
                  <div className={`status ${getStatusClass(service.status)}`}>
                    {service.status === 'running' ? '정상' : 
                     service.status === 'paused' ? '지연' : 
                     service.status === 'stopped' ? '정지' : '오류'}
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* 고객사 현황 */}
        <div className="dashboard-widget">
          <div className="widget-header">
            <div className="widget-title">
              <i className="fas fa-building text-primary-600"></i>
              고객사 현황
            </div>
          </div>
          <div className="widget-content">
            <div className="stats-grid">
              <div className="stat-card">
                <div className="stat-number">{tenantStats.activeTenants}</div>
                <div className="stat-label">활성 테넌트</div>
              </div>
              <div className="stat-card">
                <div className="stat-number">{tenantStats.edgeServers}</div>
                <div className="stat-label">Edge 서버</div>
              </div>
              <div className="stat-card">
                <div className="stat-number">{tenantStats.connectedDevices}</div>
                <div className="stat-label">연결된 디바이스</div>
              </div>
              <div className="stat-card">
                <div className="stat-number">{tenantStats.uptime}%</div>
                <div className="stat-label">가동률</div>
              </div>
            </div>
          </div>
        </div>

        {/* 데이터 처리 성능 */}
        <div className="dashboard-widget">
          <div className="widget-header">
            <div className="widget-title">
              <i className="fas fa-chart-line text-primary-600"></i>
              데이터 처리 성능
            </div>
          </div>
          <div className="widget-content">
            <div className="metric-item">
              <div className="metric-label">
                <i className="fas fa-tachometer-alt text-primary-500"></i>
                초당 데이터 포인트
              </div>
              <div className="metric-value">{metrics.dataPointsPerSecond.toLocaleString()}</div>
            </div>
            <div className="metric-item">
              <div className="metric-label">
                <i className="fas fa-clock text-success-500"></i>
                평균 응답 시간
              </div>
              <div className="metric-value">{metrics.avgResponseTime}ms</div>
            </div>
            <div className="metric-item">
              <div className="metric-label">
                <i className="fas fa-database text-warning-500"></i>
                DB 쿼리 시간
              </div>
              <div className="metric-value">{metrics.dbQueryTime}ms</div>
            </div>
            <div className="chart-container">
              <div className="chart-placeholder">
                <i className="fas fa-chart-area text-4xl mb-2"></i>
                <div>실시간 성능 차트</div>
              </div>
            </div>
          </div>
        </div>

        {/* 시스템 리소스 */}
        <div className="dashboard-widget">
          <div className="widget-header">
            <div className="widget-title">
              <i className="fas fa-microchip text-primary-600"></i>
              시스템 리소스
            </div>
          </div>
          <div className="widget-content">
            <div className="metric-item">
              <div className="metric-label">
                <i className="fas fa-microchip text-primary-500"></i>
                CPU 사용률
              </div>
              <div className="metric-value">{metrics.cpuUsage}%</div>
            </div>
            <div className="progress-bar">
              <div className={`progress-fill ${getProgressBarClass(metrics.cpuUsage)}`} 
                   style={{ width: `${metrics.cpuUsage}%` }}></div>
            </div>

            <div className="metric-item">
              <div className="metric-label">
                <i className="fas fa-memory text-warning-500"></i>
                메모리 사용률
              </div>
              <div className="metric-value">{metrics.memoryUsage}%</div>
            </div>
            <div className="progress-bar">
              <div className={`progress-fill ${getProgressBarClass(metrics.memoryUsage)}`} 
                   style={{ width: `${metrics.memoryUsage}%` }}></div>
            </div>

            <div className="metric-item">
              <div className="metric-label">
                <i className="fas fa-hdd text-success-500"></i>
                디스크 사용률
              </div>
              <div className="metric-value">{metrics.diskUsage}%</div>
            </div>
            <div className="progress-bar">
              <div className={`progress-fill ${getProgressBarClass(metrics.diskUsage)}`} 
                   style={{ width: `${metrics.diskUsage}%` }}></div>
            </div>

            <div className="metric-item">
              <div className="metric-label">
                <i className="fas fa-wifi text-primary-500"></i>
                네트워크 사용률
              </div>
              <div className="metric-value">{metrics.networkUsage}%</div>
            </div>
            <div className="progress-bar">
              <div className={`progress-fill ${getProgressBarClass(metrics.networkUsage)}`} 
                   style={{ width: `${metrics.networkUsage}%` }}></div>
            </div>
          </div>
        </div>

        {/* 활성 알람 */}
        <div className="dashboard-widget">
          <div className="widget-header">
            <div className="widget-title">
              <i className="fas fa-bell text-primary-600"></i>
              활성 알람
            </div>
            <span className="status status-error">
              <div className="status-dot status-dot-error"></div>
              {alarms.length}개 알람
            </span>
          </div>
          <div className="widget-content">
            {alarms.map((alarm) => (
              <div key={alarm.id} className={`alert-item ${alarm.type}`}>
                <i className={`${alarm.icon} text-${alarm.type === 'error' ? 'error' : 
                  alarm.type === 'warning' ? 'warning' : 'primary'}-500`}></i>
                <div className="flex-1">
                  <div className="alert-message">{alarm.message}</div>
                  <div className="alert-time">{formatTimeAgo(alarm.timestamp)}</div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* 연결된 디바이스 목록 */}
      <div className="dashboard-widget">
        <div className="widget-header">
          <div className="widget-title">
            <i className="fas fa-network-wired text-primary-600"></i>
            연결된 디바이스 현황
          </div>
          <button className="btn btn-sm btn-outline">
            <i className="fas fa-plus"></i> 디바이스 추가
          </button>
        </div>
        <div className="widget-content">
          <div className="device-list">
            {devices.map((device) => (
              <div key={device.id} className="device-item">
                <div className="device-info">
                  <div className={`status-dot ${getStatusDotClass(device.status)}`}></div>
                  <div>
                    <div className="device-name">{device.name}</div>
                    <div className="device-type">{device.type}</div>
                  </div>
                </div>
                <div className="metric-value">
                  {device.status === 'connected' ? `${device.dataPoints} 포인트` : '연결 실패'}
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};

export default Dashboard;