import React, { useState, useEffect } from 'react';

interface ServiceStatus {
  name: string;
  displayName: string;
  status: 'running' | 'stopped' | 'error';
  platform?: string;
  icon: string;
  pid?: number;
  controllable: boolean;
  description: string;
  exists?: boolean;
  executablePath?: string;
}

const Dashboard: React.FC = () => {
  const [services, setServices] = useState<ServiceStatus[]>([]);
  const [alarms, setAlarms] = useState<any[]>([]);
  const [devices, setDevices] = useState<any[]>([]);
  const [tenantStats, setTenantStats] = useState({
    activeTenants: 0,
    edgeServers: 0,
    connectedDevices: 0,
    uptime: 0
  });
  const [metrics, setMetrics] = useState({
    dataPointsPerSecond: 0,
    avgResponseTime: 0,
    dbQueryTime: 0
  });
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [isRefreshing, setIsRefreshing] = useState(false);

  useEffect(() => {
    loadData();
    const interval = setInterval(loadData, 10000);
    return () => clearInterval(interval);
  }, []);

  const loadData = async () => {
    try {
      setIsRefreshing(true);
      
      // 시뮬레이션 데이터 (더 많은 서비스)
      const mockServices = [
        {
          name: 'backend-api',
          displayName: 'Backend API',
          status: 'running' as const,
          platform: 'linux',
          icon: 'fas fa-server',
          pid: 1234,
          controllable: false,
          description: 'REST API 서버',
          exists: true
        },
        {
          name: 'data-collector',
          displayName: 'Data Collector',
          status: 'stopped' as const,
          platform: 'linux',
          icon: 'fas fa-download',
          controllable: true,
          description: 'C++ 데이터 수집 서비스',
          exists: false,
          executablePath: '/app/collector/bin/collector'
        },
        {
          name: 'redis-server',
          displayName: 'Redis Server',
          status: 'running' as const,
          platform: 'linux',
          icon: 'fas fa-database',
          pid: 5678,
          controllable: true,
          description: '인메모리 데이터베이스',
          exists: true
        },
        {
          name: 'postgresql',
          displayName: 'PostgreSQL',
          status: 'running' as const,
          platform: 'linux',
          icon: 'fas fa-database',
          pid: 9012,
          controllable: true,
          description: '관계형 데이터베이스',
          exists: true
        },
        {
          name: 'influxdb',
          displayName: 'InfluxDB',
          status: 'running' as const,
          platform: 'linux',
          icon: 'fas fa-chart-line',
          pid: 3456,
          controllable: true,
          description: '시계열 데이터베이스',
          exists: true
        },
        {
          name: 'rabbitmq',
          displayName: 'RabbitMQ',
          status: 'stopped' as const,
          platform: 'linux',
          icon: 'fas fa-exchange-alt',
          controllable: true,
          description: '메시지 큐 서버',
          exists: true
        }
      ];

      setServices(mockServices);

      setTenantStats({
        activeTenants: 3,
        edgeServers: 8,
        connectedDevices: 247,
        uptime: 99.7
      });

      setMetrics({
        dataPointsPerSecond: Math.floor(Math.random() * 1000) + 2000,
        avgResponseTime: Math.floor(Math.random() * 50) + 120,
        dbQueryTime: Math.floor(Math.random() * 20) + 15
      });

      // 더 많은 알람 데이터 추가 (테스트용)
      setAlarms([
        {
          id: 1,
          type: 'error',
          message: 'Device-001 연결 실패',
          timestamp: new Date(Date.now() - 5 * 60 * 1000)
        },
        {
          id: 2,
          type: 'warning',
          message: '온도 센서 값이 임계치 접근',
          timestamp: new Date(Date.now() - 15 * 60 * 1000)
        },
        {
          id: 3,
          type: 'error',
          message: 'BACnet-HVAC 통신 오류',
          timestamp: new Date(Date.now() - 25 * 60 * 1000)
        },
        {
          id: 4,
          type: 'warning',
          message: 'PLC-002 응답 지연',
          timestamp: new Date(Date.now() - 35 * 60 * 1000)
        },
        {
          id: 5,
          type: 'info',
          message: '에너지 미터 데이터 갱신',
          timestamp: new Date(Date.now() - 45 * 60 * 1000)
        },
        {
          id: 6,
          type: 'warning',
          message: '진동 모니터 값 증가',
          timestamp: new Date(Date.now() - 55 * 60 * 1000)
        }
      ]);

      // 더 많은 디바이스 데이터 추가 (테스트용)
      setDevices([
        {
          id: 1,
          name: 'PLC-001',
          type: 'Modbus TCP',
          status: 'connected',
          dataPoints: 45
        },
        {
          id: 2,
          name: 'MQTT-Sensor-01',
          type: 'MQTT',
          status: 'connected',
          dataPoints: 12
        },
        {
          id: 3,
          name: 'BACnet-HVAC',
          type: 'BACnet',
          status: 'disconnected',
          dataPoints: 0
        },
        {
          id: 4,
          name: 'PLC-002',
          type: 'Modbus RTU',
          status: 'connected',
          dataPoints: 32
        },
        {
          id: 5,
          name: 'Energy-Meter-01',
          type: 'Modbus TCP',
          status: 'connected',
          dataPoints: 24
        },
        {
          id: 6,
          name: 'Temperature-Sensor-Array',
          type: 'MQTT',
          status: 'connected',
          dataPoints: 8
        },
        {
          id: 7,
          name: 'HVAC-Controller-02',
          type: 'BACnet',
          status: 'connected',
          dataPoints: 16
        },
        {
          id: 8,
          name: 'Vibration-Monitor',
          type: 'Ethernet/IP',
          status: 'disconnected',
          dataPoints: 0
        }
      ]);

      setLastUpdate(new Date());
      
    } catch (error) {
      console.error('Data loading failed:', error);
    } finally {
      setIsRefreshing(false);
    }
  };

  const handleRefresh = () => {
    loadData();
  };

  const handleServiceControl = async (service: ServiceStatus, action: 'start' | 'stop' | 'restart') => {
    if (!service.controllable) {
      alert(`${service.displayName}는 필수 서비스로 제어할 수 없습니다.`);
      return;
    }

    try {
      setIsRefreshing(true);
      
      // 🚀 실제 백엔드 API 호출
      let result;
      switch (action) {
        case 'start':
          result = await fetch('/api/services/start', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ service: service.name })
          }).then(res => res.json());
          break;
        case 'stop':
          result = await fetch('/api/services/stop', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ service: service.name })
          }).then(res => res.json());
          break;
        case 'restart':
          result = await fetch('/api/services/restart', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ service: service.name })
          }).then(res => res.json());
          break;
      }

      if (result.success) {
        alert(`${service.displayName} ${action} 성공!`);
        setTimeout(loadData, 2000); // 2초 후 데이터 새로고침
      } else {
        alert(`${action} 실패: ${result.error || result.message}`);
      }
    } catch (error) {
      console.error('Service control error:', error);
      alert(`서비스 제어 중 오류가 발생했습니다: ${error}`);
    } finally {
      setIsRefreshing(false);
    }
  };

  const formatTimeAgo = (date: Date) => {
    const diff = Math.floor((Date.now() - date.getTime()) / 60000);
    return diff < 1 ? '방금 전' : diff < 60 ? `${diff}분 전` : `${Math.floor(diff/60)}시간 전`;
  };

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

        {service.pid && (
          <div className="service-pid">PID: {service.pid}</div>
        )}

        <div className="service-controls">
          {service.controllable ? (
            <>
              {isRunning ? (
                <div className="control-buttons">
                  <button 
                    className="btn btn-sm btn-warning"
                    onClick={() => handleServiceControl(service, 'restart')}
                    disabled={isRefreshing}
                  >
                    <i className="fas fa-redo"></i>
                    재시작
                  </button>
                  <button 
                    className="btn btn-sm btn-error"
                    onClick={() => handleServiceControl(service, 'stop')}
                    disabled={isRefreshing}
                  >
                    <i className="fas fa-stop"></i>
                    정지
                  </button>
                </div>
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
        
        <div className="service-description">
          {service.description}
        </div>

        {service.exists === false && (
          <div className="warning-box">
            <i className="fas fa-exclamation-triangle"></i>
            실행 파일이 없습니다
            <div className="file-path">{service.executablePath}</div>
          </div>
        )}
      </div>
    );
  };

  return (
    <div className="dashboard-container">
      {/* 대시보드 헤더 */}
      <div className="dashboard-header">
        <h1 className="dashboard-title">🚀 PulseOne 시스템 대시보드</h1>
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

      {/* 🎯 개선된 레이아웃 - 2x2 그리드 구조 */}
      <div className="dashboard-improved-grid">
        {/* 상단 행 */}
        <div className="top-row">
          {/* 왼쪽: 서비스 상태 */}
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
              <div className="service-grid-compact">
                {services.map(service => renderServiceCard(service))}
              </div>
            </div>
          </div>

          {/* 오른쪽: 시스템 현황 + 데이터 처리 성능 */}
          <div className="top-right-column">
            {/* 시스템 현황 */}
            <div className="dashboard-widget">
              <div className="widget-header">
                <div className="widget-title">
                  <i className="fas fa-chart-pie text-primary-600"></i>
                  시스템 현황
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
              </div>
            </div>
          </div>
        </div>

        {/* 하단 행 */}
        <div className="bottom-row">
          {/* 왼쪽: 연결된 디바이스 현황 */}
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
                  <div key={device.id} className="device-compact-item">
                    <div className={`status-dot ${device.status === 'connected' ? 'status-connected' : 'status-disconnected'}`}></div>
                    <div className="device-info-compact">
                      <div className="device-name">{device.name}</div>
                      <div className="device-type">{device.type}</div>
                    </div>
                    <div className="device-points">
                      {device.status === 'connected' ? `${device.dataPoints} 포인트` : '연결 실패'}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>

          {/* 오른쪽: 최근 알람 */}
          <div className="dashboard-widget">
            <div className="widget-header">
              <div className="widget-title">
                <i className="fas fa-bell text-primary-600"></i>
                최근 알람
              </div>
              <button className="btn btn-sm btn-outline">
                <i className="fas fa-cog"></i> 설정
              </button>
            </div>
            <div className="widget-content">
              <div className="alarm-list">
                {alarms.map((alarm) => (
                  <div key={alarm.id} className={`alert-item ${alarm.type}`}>
                    <i className={`fas fa-${alarm.type === 'error' ? 'exclamation-circle' : 
                      alarm.type === 'warning' ? 'exclamation-triangle' : 'info-circle'
                    } text-${alarm.type === 'error' ? 'error' : 
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
        </div>
      </div>

      <style jsx>{`
        .dashboard-container {
          width: 100%;
          max-width: none;
          padding: 1.5rem;
          background: #f8fafc;
          min-height: 100vh;
        }

        .dashboard-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 1.5rem;
          padding-bottom: 1rem;
          border-bottom: 1px solid #e2e8f0;
        }

        .dashboard-title {
          font-size: 1.875rem;
          font-weight: 700;
          color: #1e293b;
          margin: 0;
        }

        .dashboard-actions {
          display: flex;
          gap: 0.75rem;
        }

        .form-select {
          padding: 0.5rem 1rem;
          border: 1px solid #e2e8f0;
          border-radius: 0.5rem;
          background: white;
          font-size: 0.875rem;
        }

        .dashboard-status-bar {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 1.5rem;
          padding: 1rem;
          background: white;
          border-radius: 0.5rem;
          box-shadow: 0 1px 3px rgba(0,0,0,0.1);
          border: 1px solid #e2e8f0;
        }

        .connection-status {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          color: #059669;
          font-weight: 500;
        }

        .live-indicator {
          width: 8px;
          height: 8px;
          border-radius: 50%;
          background: #10b981;
          animation: pulse 2s infinite;
        }

        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }

        .dashboard-controls {
          display: flex;
          align-items: center;
          gap: 1rem;
        }

        .refresh-btn {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          background: #0ea5e9;
          color: white;
          border: none;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          cursor: pointer;
          transition: all 0.2s;
        }

        .refresh-btn:hover {
          background: #0284c7;
        }

        .refresh-btn:disabled {
          opacity: 0.6;
          cursor: not-allowed;
        }

        /* 🎯 완벽한 2x2 그리드 레이아웃 - 스크롤 가능 */
        .dashboard-improved-grid {
          display: grid;
          grid-template-rows: auto auto;
          gap: 1.5rem;
          min-height: 600px;
        }

        .top-row,
        .bottom-row {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 1.5rem;
          min-height: 350px;
        }

        .top-right-column {
          display: flex;
          flex-direction: column;
          gap: 1rem;
          height: 100%;
        }

        .top-right-column .dashboard-widget:first-child {
          flex: 1;
        }

        .top-right-column .dashboard-widget:last-child {
          flex: 1;
        }

        .dashboard-widget {
          background: white;
          border-radius: 0.5rem;
          box-shadow: 0 1px 3px rgba(0,0,0,0.1);
          border: 1px solid #e2e8f0;
          overflow: hidden;
          transition: all 0.2s ease-in-out;
          display: flex;
          flex-direction: column;
          min-height: 300px;
        }

        .dashboard-widget:hover {
          box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }

        .widget-header {
          padding: 1rem;
          background: #f8fafc;
          border-bottom: 1px solid #e2e8f0;
          display: flex;
          align-items: center;
          justify-content: space-between;
        }

        .widget-title {
          font-size: 1.125rem;
          font-weight: 600;
          color: #374151;
          display: flex;
          align-items: center;
          gap: 0.5rem;
        }

        .widget-content {
          padding: 1rem;
          flex: 1;
          overflow: hidden;
        }

        /* 하단 위젯들 가로 배치 */
        .bottom-widgets-row {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 1.5rem;
        }
        .service-grid-compact {
          display: grid;
          grid-template-columns: repeat(2, 1fr);
          gap: 0.75rem;
          max-height: 500px;
          overflow-y: auto;
        }

        .service-card {
          padding: 0.875rem;
          border: 1px solid #e2e8f0;
          border-radius: 0.5rem;
          text-align: center;
          transition: all 0.2s;
          background: #fafafa;
        }

        .service-card:hover {
          transform: translateY(-2px);
          box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }

        .service-icon {
          font-size: 1.5rem;
          margin-bottom: 0.5rem;
        }

        .service-name {
          font-size: 0.8rem;
          font-weight: 500;
          color: #374151;
          margin-bottom: 0.25rem;
        }

        .service-pid {
          font-size: 0.7rem;
          color: #6b7280;
          margin-bottom: 0.5rem;
          font-family: monospace;
        }

        .status {
          display: inline-block;
          padding: 0.2rem 0.4rem;
          border-radius: 9999px;
          font-size: 0.7rem;
          font-weight: 500;
          margin-bottom: 0.5rem;
        }

        .status-running {
          background: #dcfce7;
          color: #166534;
        }

        .status-stopped {
          background: #fef3c7;
          color: #92400e;
        }

        .status-error {
          background: #fee2e2;
          color: #991b1b;
        }

        .service-controls {
          margin-bottom: 0.5rem;
        }

        .control-buttons {
          display: flex;
          gap: 0.25rem;
          justify-content: center;
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 0.25rem;
          padding: 0.375rem 0.5rem;
          border: none;
          border-radius: 0.375rem;
          font-size: 0.7rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s;
        }

        .btn-sm {
          padding: 0.25rem 0.375rem;
          font-size: 0.65rem;
        }

        .btn-success {
          background: #10b981;
          color: white;
        }

        .btn-success:hover {
          background: #059669;
        }

        .btn-warning {
          background: #f59e0b;
          color: white;
        }

        .btn-warning:hover {
          background: #d97706;
        }

        .btn-error {
          background: #ef4444;
          color: white;
        }

        .btn-error:hover {
          background: #dc2626;
        }

        .btn-outline {
          background: transparent;
          color: #6b7280;
          border: 1px solid #d1d5db;
        }

        .btn-outline:hover {
          background: #f9fafb;
        }

        .btn:disabled {
          opacity: 0.6;
          cursor: not-allowed;
        }

        .service-description {
          margin-top: 0.25rem;
          font-size: 0.65rem;
          color: #6b7280;
          text-align: center;
          line-height: 1.2;
        }

        .warning-box {
          margin-top: 0.5rem;
          padding: 0.375rem;
          background: #fffbeb;
          border: 1px solid #fed7aa;
          border-radius: 0.375rem;
          color: #d97706;
          font-size: 0.65rem;
          text-align: center;
        }

        .file-path {
          font-family: monospace;
          font-size: 0.55rem;
          color: #92400e;
          margin-top: 0.25rem;
        }

        /* 시스템 현황 그리드 */
        .stats-grid {
          display: grid;
          grid-template-columns: repeat(2, 1fr);
          gap: 1rem;
        }

        .stat-card {
          text-align: center;
          padding: 0.75rem;
          background: #f8fafc;
          border-radius: 0.375rem;
        }

        .stat-number {
          font-size: 1.5rem;
          font-weight: 700;
          color: #0ea5e9;
        }

        .stat-label {
          font-size: 0.75rem;
          color: #6b7280;
          text-transform: uppercase;
          letter-spacing: 0.5px;
        }

        /* 메트릭 아이템 */
        .metric-item {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 0.75rem 0;
          border-bottom: 1px solid #f1f5f9;
        }

        .metric-item:last-child {
          border-bottom: none;
        }

        .metric-label {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          font-size: 0.875rem;
          color: #374151;
        }

        .metric-value {
          font-weight: 600;
          color: #1e293b;
          text-align: right;
          white-space: nowrap;
          background: #f0f9ff;
          padding: 0.25rem 0.75rem;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          border: 1px solid #e0f2fe;
        }

        /* 알람 리스트도 함께 조정 */
        .alarm-list {
          max-height: 400px;
          overflow-y: auto;
        }

        .alert-item {
          display: flex;
          align-items: center;
          gap: 0.75rem;
          padding: 0.75rem;
          background: #f8fafc;
          border-radius: 0.375rem;
          margin-bottom: 0.5rem;
          border-left: 4px solid #ef4444;
        }

        .alert-item.warning {
          border-left-color: #f59e0b;
        }

        .alert-item.info {
          border-left-color: #0ea5e9;
        }

        .alert-time {
          font-size: 0.75rem;
          color: #6b7280;
        }

        .alert-message {
          font-size: 0.875rem;
          color: #374151;
        }

        /* 디바이스 리스트 */
        .device-list {
          max-height: 400px;
          overflow-y: auto;
        }

        .device-compact-item {
          display: flex;
          align-items: center;
          gap: 0.75rem;
          padding: 0.75rem;
          background: #ffffff;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          margin-bottom: 0.5rem;
        }

        .status-dot {
          width: 8px;
          height: 8px;
          border-radius: 50%;
          flex-shrink: 0;
        }

        .status-connected {
          background: #10b981;
        }

        .status-disconnected {
          background: #ef4444;
        }

        .device-info-compact {
          flex: 1;
        }

        .device-name {
          font-weight: 600;
          color: #111827;
          font-size: 0.875rem;
        }

        .device-type {
          font-size: 0.75rem;
          color: #6b7280;
        }

        .device-points {
          font-weight: 600;
          font-size: 0.75rem;
          color: #059669;
        }

        .service-summary {
          display: flex;
          align-items: center;
          gap: 0.5rem;
        }

        .text-sm {
          font-size: 0.875rem;
        }

        .text-xs {
          font-size: 0.75rem;
        }

        .text-success-600 {
          color: #059669;
        }

        .text-error-600 {
          color: #dc2626;
        }

        .text-neutral-400 {
          color: #9ca3af;
        }

        .text-neutral-500 {
          color: #6b7280;
        }

        .text-neutral-600 {
          color: #4b5563;
        }

        .text-primary-600 {
          color: #0284c7;
        }

        .text-primary-500 {
          color: #0ea5e9;
        }

        .text-success-500 {
          color: #10b981;
        }

        .text-warning-500 {
          color: #f59e0b;
        }

        .text-error-500 {
          color: #ef4444;
        }

        .ml-2 {
          margin-left: 0.5rem;
        }

        .flex-1 {
          flex: 1;
        }

        /* 반응형 디자인 */
        @media (max-width: 1200px) {
          .dashboard-improved-grid {
            grid-template-columns: 1fr;
            gap: 1rem;
          }
          
          .bottom-widgets-row {
            grid-template-columns: 1fr;
          }
        }

        @media (max-width: 768px) {
          .service-grid-compact {
            grid-template-columns: repeat(2, 1fr);
          }
          
          .stats-grid {
            grid-template-columns: repeat(2, 1fr);
          }
          
          .bottom-widgets-row {
            grid-template-columns: 1fr;
          }
        }

        @media (max-width: 480px) {
          .service-grid-compact {
            grid-template-columns: 1fr;
          }
          
          .stats-grid {
            grid-template-columns: 1fr;
          }
        }
      `}</style>
    </div>
  );
};

export default Dashboard;