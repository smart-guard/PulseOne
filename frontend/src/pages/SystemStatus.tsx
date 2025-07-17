import React, { useState, useEffect } from 'react';
import '../styles/base.css';

interface ServiceStatus {
  name: string;
  displayName: string;
  status: 'running' | 'stopped' | 'error';
  icon: string;
  controllable: boolean;
  container: string;
  description: string;
  ports: string[];
  uptime: string;
  memoryUsage: string;
  cpuUsage: string;
}

const SystemStatus: React.FC = () => {
  const [services, setServices] = useState<ServiceStatus[]>([
    { 
      name: 'backend', 
      displayName: 'Backend API',
      status: 'running', 
      icon: 'fas fa-server',
      controllable: false,
      container: 'pulseone-backend-dev',
      description: 'REST API 서버 (필수 서비스)',
      ports: ['3000:3000'],
      uptime: '2h 45m',
      memoryUsage: '256MB',
      cpuUsage: '15%'
    },
    { 
      name: 'collector', 
      displayName: 'Data Collector',
      status: 'running', 
      icon: 'fas fa-download',
      controllable: true,
      container: 'pulseone-collector-dev',
      description: 'C++ 데이터 수집 서비스',
      ports: [],
      uptime: '2h 45m',
      memoryUsage: '128MB',
      cpuUsage: '8%'
    },
    { 
      name: 'redis', 
      displayName: 'Redis Cache',
      status: 'running', 
      icon: 'fas fa-database',
      controllable: true,
      container: 'pulseone-redis',
      description: '실시간 데이터 캐시',
      ports: ['6379:6379'],
      uptime: '2h 45m',
      memoryUsage: '64MB',
      cpuUsage: '2%'
    },
    { 
      name: 'postgres', 
      displayName: 'PostgreSQL',
      status: 'running', 
      icon: 'fas fa-database',
      controllable: true,
      container: 'pulseone-postgres',
      description: '메인 데이터베이스',
      ports: ['5432:5432'],
      uptime: '2h 45m',
      memoryUsage: '512MB',
      cpuUsage: '5%'
    },
    { 
      name: 'influxdb', 
      displayName: 'InfluxDB',
      status: 'running', 
      icon: 'fas fa-chart-line',
      controllable: true,
      container: 'pulseone-influx',
      description: '시계열 데이터베이스',
      ports: ['8086:8086'],
      uptime: '2h 45m',
      memoryUsage: '384MB',
      cpuUsage: '3%'
    },
    { 
      name: 'rabbitmq', 
      displayName: 'RabbitMQ',
      status: 'running', 
      icon: 'fas fa-exchange-alt',
      controllable: true,
      container: 'pulseone-rabbitmq',
      description: '메시지 큐 브로커',
      ports: ['5672:5672', '15672:15672'],
      uptime: '2h 45m',
      memoryUsage: '192MB',
      cpuUsage: '4%'
    }
  ]);

  const [isProcessing, setIsProcessing] = useState(false);
  const [selectedServices, setSelectedServices] = useState<string[]>([]);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  // 실시간 업데이트
  useEffect(() => {
    const interval = setInterval(() => {
      setLastUpdate(new Date());
      // 실제로는 여기서 서비스 상태 API 호출
    }, 10000);

    return () => clearInterval(interval);
  }, []);

  // 개별 서비스 제어
  const handleServiceControl = async (service: ServiceStatus, action: 'start' | 'stop' | 'restart') => {
    if (!service.controllable) {
      alert(`${service.displayName}는 필수 서비스로 제어할 수 없습니다.`);
      return;
    }

    try {
      setIsProcessing(true);
      
      console.log(`${action}ing ${service.displayName}...`);
      
      // 시뮬레이션
      await new Promise(resolve => setTimeout(resolve, 2000));
      
      setServices(prev => prev.map(s => 
        s.name === service.name 
          ? { 
              ...s, 
              status: action === 'stop' ? 'stopped' : 'running',
              uptime: action === 'stop' ? '0m' : '0m'
            }
          : s
      ));
      
      console.log(`${service.displayName} ${action} 완료`);
    } catch (error) {
      console.error(`서비스 제어 실패:`, error);
      alert(`${service.displayName} ${action} 실패`);
    } finally {
      setIsProcessing(false);
    }
  };

  // 전체 서비스 제어
  const handleBulkAction = async (action: 'start' | 'stop' | 'restart') => {
    const controllableServices = services.filter(s => s.controllable);
    
    if (controllableServices.length === 0) {
      alert('제어 가능한 서비스가 없습니다.');
      return;
    }

    const confirmation = confirm(
      `${controllableServices.length}개의 서비스를 ${
        action === 'start' ? '시작' : 
        action === 'stop' ? '정지' : '재시작'
      }하시겠습니까?`
    );

    if (!confirmation) return;

    try {
      setIsProcessing(true);
      
      // 순차적으로 처리
      for (const service of controllableServices) {
        console.log(`${action}ing ${service.displayName}...`);
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        setServices(prev => prev.map(s => 
          s.name === service.name 
            ? { 
                ...s, 
                status: action === 'stop' ? 'stopped' : 'running',
                uptime: action === 'stop' ? '0m' : '0m'
              }
            : s
        ));
      }
      
      alert(`${controllableServices.length}개 서비스 ${action} 완료`);
    } catch (error) {
      console.error('일괄 서비스 제어 실패:', error);
      alert('일괄 서비스 제어 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // 선택된 서비스 제어
  const handleSelectedAction = async (action: 'start' | 'stop' | 'restart') => {
    if (selectedServices.length === 0) {
      alert('서비스를 선택해주세요.');
      return;
    }

    const selectedServiceObjects = services.filter(s => 
      selectedServices.includes(s.name) && s.controllable
    );

    if (selectedServiceObjects.length === 0) {
      alert('제어 가능한 서비스가 선택되지 않았습니다.');
      return;
    }

    try {
      setIsProcessing(true);
      
      for (const service of selectedServiceObjects) {
        console.log(`${action}ing ${service.displayName}...`);
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        setServices(prev => prev.map(s => 
          s.name === service.name 
            ? { 
                ...s, 
                status: action === 'stop' ? 'stopped' : 'running',
                uptime: action === 'stop' ? '0m' : '0m'
              }
            : s
        ));
      }
      
      alert(`${selectedServiceObjects.length}개 서비스 ${action} 완료`);
      setSelectedServices([]);
    } catch (error) {
      console.error('선택된 서비스 제어 실패:', error);
      alert('선택된 서비스 제어 실패');
    } finally {
      setIsProcessing(false);
    }
  };

  // 체크박스 핸들러
  const handleServiceSelect = (serviceName: string) => {
    setSelectedServices(prev => 
      prev.includes(serviceName)
        ? prev.filter(name => name !== serviceName)
        : [...prev, serviceName]
    );
  };

  const formatTimeAgo = (date: Date) => {
    const now = new Date();
    const diffMs = now.getTime() - date.getTime();
    const diffSecs = Math.floor(diffMs / 1000);
    
    if (diffSecs < 60) return '방금 전';
    const diffMins = Math.floor(diffSecs / 60);
    if (diffMins < 60) return `${diffMins}분 전`;
    const diffHours = Math.floor(diffMins / 60);
    return `${diffHours}시간 전`;
  };

  return (
    <div className="service-management-container">
      {/* 페이지 헤더 */}
      <div className="page-header">
        <h1 className="page-title">서비스 관리</h1>
        <div className="page-actions">
          <span className="text-sm text-neutral-600">
            마지막 업데이트: {formatTimeAgo(lastUpdate)}
          </span>
          <button 
            className="btn btn-primary"
            onClick={() => setLastUpdate(new Date())}
            disabled={isProcessing}
          >
            <i className="fas fa-sync-alt"></i>
            새로고침
          </button>
        </div>
      </div>

      {/* 일괄 제어 패널 */}
      <div className="control-panel">
        <div className="control-section">
          <h3 className="section-title">전체 서비스 제어</h3>
          <div className="control-buttons">
            <button 
              className="btn btn-success"
              onClick={() => handleBulkAction('start')}
              disabled={isProcessing}
            >
              <i className="fas fa-play"></i>
              전체 시작
            </button>
            <button 
              className="btn btn-warning"
              onClick={() => handleBulkAction('restart')}
              disabled={isProcessing}
            >
              <i className="fas fa-redo"></i>
              전체 재시작
            </button>
            <button 
              className="btn btn-error"
              onClick={() => handleBulkAction('stop')}
              disabled={isProcessing}
            >
              <i className="fas fa-stop"></i>
              전체 정지
            </button>
          </div>
        </div>

        <div className="control-section">
          <h3 className="section-title">선택된 서비스 제어</h3>
          <div className="selected-info">
            <span className="text-sm text-neutral-600">
              {selectedServices.length}개 서비스 선택됨
            </span>
          </div>
          <div className="control-buttons">
            <button 
              className="btn btn-success btn-sm"
              onClick={() => handleSelectedAction('start')}
              disabled={isProcessing || selectedServices.length === 0}
            >
              <i className="fas fa-play"></i>
              선택 시작
            </button>
            <button 
              className="btn btn-warning btn-sm"
              onClick={() => handleSelectedAction('restart')}
              disabled={isProcessing || selectedServices.length === 0}
            >
              <i className="fas fa-redo"></i>
              선택 재시작
            </button>
            <button 
              className="btn btn-error btn-sm"
              onClick={() => handleSelectedAction('stop')}
              disabled={isProcessing || selectedServices.length === 0}
            >
              <i className="fas fa-stop"></i>
              선택 정지
            </button>
          </div>
        </div>
      </div>

      {/* 서비스 목록 */}
      <div className="service-list">
        <div className="service-list-header">
          <div className="service-list-title">
            <h3>서비스 목록</h3>
            <span className="service-count">
              {services.filter(s => s.status === 'running').length} / {services.length} 실행 중
            </span>
          </div>
        </div>

        <div className="service-table">
          <div className="service-table-header">
            <div className="service-table-cell">
              <input 
                type="checkbox" 
                onChange={(e) => {
                  if (e.target.checked) {
                    setSelectedServices(services.filter(s => s.controllable).map(s => s.name));
                  } else {
                    setSelectedServices([]);
                  }
                }}
                checked={selectedServices.length === services.filter(s => s.controllable).length}
              />
            </div>
            <div className="service-table-cell">서비스</div>
            <div className="service-table-cell">상태</div>
            <div className="service-table-cell">업타임</div>
            <div className="service-table-cell">리소스</div>
            <div className="service-table-cell">포트</div>
            <div className="service-table-cell">제어</div>
          </div>

          {services.map((service) => (
            <div key={service.name} className="service-table-row">
              <div className="service-table-cell">
                <input 
                  type="checkbox"
                  checked={selectedServices.includes(service.name)}
                  onChange={() => handleServiceSelect(service.name)}
                  disabled={!service.controllable}
                />
              </div>
              
              <div className="service-table-cell">
                <div className="service-info">
                  <div className={`service-icon ${
                    service.status === 'running' ? 'text-success-600' : 
                    service.status === 'error' ? 'text-error-600' : 'text-neutral-400'
                  }`}>
                    <i className={service.icon}></i>
                  </div>
                  <div>
                    <div className="service-name">{service.displayName}</div>
                    <div className="service-description">{service.description}</div>
                  </div>
                </div>
              </div>
              
              <div className="service-table-cell">
                <span className={`status ${
                  service.status === 'running' ? 'status-running' : 
                  service.status === 'error' ? 'status-error' : 'status-stopped'
                }`}>
                  <div className={`status-dot ${
                    service.status === 'running' ? 'status-dot-running' : 
                    service.status === 'error' ? 'status-dot-error' : 'status-dot-stopped'
                  }`}></div>
                  {service.status === 'running' ? '실행 중' : 
                   service.status === 'error' ? '오류' : '정지'}
                </span>
              </div>
              
              <div className="service-table-cell">
                <span className="text-sm text-neutral-600">{service.uptime}</span>
              </div>
              
              <div className="service-table-cell">
                <div className="resource-info">
                  <span className="text-xs text-neutral-500">CPU: {service.cpuUsage}</span>
                  <span className="text-xs text-neutral-500">MEM: {service.memoryUsage}</span>
                </div>
              </div>
              
              <div className="service-table-cell">
                <div className="port-list">
                  {service.ports.map((port, index) => (
                    <span key={index} className="port-badge">{port}</span>
                  ))}
                </div>
              </div>
              
              <div className="service-table-cell">
                <div className="service-controls">
                  {service.controllable ? (
                    <div className="control-buttons">
                      {service.status === 'running' ? (
                        <>
                          <button 
                            className="btn btn-sm btn-warning"
                            onClick={() => handleServiceControl(service, 'restart')}
                            disabled={isProcessing}
                          >
                            <i className="fas fa-redo"></i>
                          </button>
                          <button 
                            className="btn btn-sm btn-error"
                            onClick={() => handleServiceControl(service, 'stop')}
                            disabled={isProcessing}
                          >
                            <i className="fas fa-stop"></i>
                          </button>
                        </>
                      ) : (
                        <button 
                          className="btn btn-sm btn-success"
                          onClick={() => handleServiceControl(service, 'start')}
                          disabled={isProcessing}
                        >
                          <i className="fas fa-play"></i>
                        </button>
                      )}
                    </div>
                  ) : (
                    <span className="text-xs text-neutral-500">필수</span>
                  )}
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};

export default SystemStatus;

