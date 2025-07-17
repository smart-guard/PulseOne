import React, { useState, useEffect } from 'react';
import '../styles/base.css';

// 서비스 상태 타입 정의
interface ServiceStatus {
  name: string;
  displayName: string;
  status: 'running' | 'stopped' | 'error';
  icon: string;
  controllable: boolean;
  container: string;
  description: string;
}

// 기존 타입 정의들
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
  // 서비스 상태 관리
  const [services, setServices] = useState<ServiceStatus[]>([
    { 
      name: 'backend', 
      displayName: 'Backend API',
      status: 'running', 
      icon: 'fas fa-server',
      controllable: false,
      container: 'pulseone-backend-dev',
      description: 'REST API 서버 (필수 서비스)'
    },
    { 
      name: 'collector', 
      displayName: 'Data Collector',
      status: 'running', 
      icon: 'fas fa-download',
      controllable: true,
      container: 'pulseone-collector-dev',
      description: 'C++ 데이터 수집 서비스'
    },
    { 
      name: 'redis', 
      displayName: 'Redis Cache',
      status: 'running', 
      icon: 'fas fa-database',
      controllable: true,
      container: 'pulseone-redis',
      description: '실시간 데이터 캐시'
    },
    { 
      name: 'postgres', 
      displayName: 'PostgreSQL',
      status: 'running', 
      icon: 'fas fa-database',
      controllable: true,
      container: 'pulseone-postgres',
      description: '메인 데이터베이스'
    },
    { 
      name: 'influxdb', 
      displayName: 'InfluxDB',
      status: 'running', 
      icon: 'fas fa-chart-line',
      controllable: true,
      container: 'pulseone-influx',
      description: '시계열 데이터베이스'
    },
    { 
      name: 'rabbitmq', 
      displayName: 'RabbitMQ',
      status: 'running', 
      icon: 'fas fa-exchange-alt',
      controllable: true,
      container: 'pulseone-rabbitmq',
      description: '메시지 큐 브로커'
    }
  ]);

  // 기존 상태들
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

  // 실시간 업데이트 시뮬레이션
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

  // 새로고침 핸들러
  const handleRefresh = async () => {
    setIsRefreshing(true);
    await new Promise(resolve => setTimeout(resolve, 1000));
    setLastUpdate(new Date());
    setIsRefreshing(false);
  };

  // 서비스 제어 함수
  const handleServiceControl = async (service: ServiceStatus, action: 'start' | 'stop') => {
    if (!service.controllable) {
      alert(`${service.displayName}는 필수 서비스로 제어할 수 없습니다.`);
      return;
    }

    try {
      setIsRefreshing(true);
      
      // 시뮬레이션: 실제로는 여기서 Docker API 호출
      console.log(`${action}ing ${service.displayName}...`);
      
      // 임시 지연
      await new Promise(resolve => setTimeout(resolve, 1000));
      
      // 상태 업데이트
      setServices(prev => prev.map(s => 
        s.name === service.name 
          ? { ...s, status: action === 'start' ? 'running' : 'stopped' }
          : s
      ));
      
      console.log(`${service.displayName} ${action === 'start' ? '시작' : '정지'} 완료`);
    } catch (error) {
      console.error(`서비스 제어 실패:`, error);
      alert(`${service.displayName} ${action === 'start' ? '시작' : '정지'} 실패`);
    } finally {
      setIsRefreshing(false);
    }
  };

  // 유틸리티 함수들
  const getStatusDotClass = (status: string) => {
    switch (status) {
      case 'running':
      case 'connected':
        return 'status-dot-running';
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

  // 서비스 카드 렌더링
  const renderServiceCard = (service: ServiceStatus) => {
    const isRunning = service.status === 'running';
    const isError = service.status === 'error';
    
    return (
      <div key={service.name} className="service-card">
        <div className={`service-icon ${
          isRunning ? 'text-success-600' : 
          isError ? 'text-error-600' : 'text-neutral-400'
        }`}>
          <i className={service.icon}></i>
        </div>
        
        <div className="service-name">{service.displayName}</div>
        
        <div className={`status ${
          isRunning ? 'status-running' : 
          isError ? 'status-error' : 'status-stopped'
        }`}>
          {isRunning ? '정상' : isError ? '오류' : '정지'}
        </div>

        {/* 제어 버튼 */}
        <div className="service-controls">
          {service.controllable ? (
            <>
              {isRunning ? (
                <button 
                  className="btn btn-sm btn-error"
                  onClick={() => handleServiceControl(service, 'stop')}
                  disabled={isRefreshing}
                >
                  <i className="fas fa-stop"></i>
                  정지
                </button>
              ) : (
                <button 
                  className="btn btn-sm btn-success"
                  onClick={() => handleServiceControl(service, 'start')}
                  disabled={isRefreshing}
                >
                  <i className="fas fa-play"></i>
                  시작
                </button>
              )}
            </>
          ) : (
            <span className="text-xs text-neutral-500">필수 서비스</span>
          )}
        </div>
        
        {/* 설명 */}
        <div className="service-description">
          {service.description}
        </div>
      </div>
    );
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
        <div className="connection-status">
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
        {/* 서비스 상태 위젯 */}
        <div className="dashboard-widget">
          <div className="widget-header">
            <div className="widget-title">
              <i className="fas fa-server text-primary-600"></i>
              서비스 상태
            </div>
            <div className="service-summary">
              <span className="text-sm text-success-600">
                {services.filter(s => s.status === 'running').length}개 실행 중
              </span>
              {services.some(s => s.status === 'error') && (
                <span className="text-sm text-error-600 ml-2">
                  {services.filter(s => s.status === 'error').length}개 오류
                </span>
              )}
            </div>
          </div>
          <div className="widget-content">
            <div className="service-grid">
              {services.map(service => renderServiceCard(service))}
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
