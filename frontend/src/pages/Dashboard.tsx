// ============================================================================
// frontend/src/pages/Dashboard.tsx
// 📝 원래 의도된 레이아웃: 왼쪽 서비스 목록 + 오른쪽 2x2 시스템 상태
// ============================================================================

import React, { useState, useEffect, useCallback } from 'react';
import { ApiService } from '../api';
import '../styles/base.css';
import '../styles/dashboard.css';

// 🎯 기존 인터페이스들 그대로 유지
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
  // ============================================================================
  // 🔧 기존 상태 관리 그대로 유지  
  // ============================================================================
  
  const [dashboardData, setDashboardData] = useState<DashboardData | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  
  // 실시간 업데이트
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(10000); // 10초
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  // ============================================================================
  // 🔄 강화된 데이터 로드 함수들 - JSON 파싱 에러 해결
  // ============================================================================

  /**
   * 🛠️ 안전한 JSON 파싱 헬퍼 함수
   */
  const safeJsonParse = async (response: Response): Promise<any> => {
    const contentType = response.headers.get('content-type');
    
    if (!contentType || !contentType.includes('application/json')) {
      throw new Error(`API가 JSON이 아닌 ${contentType || 'unknown'} 형식으로 응답했습니다`);
    }

    const text = await response.text();
    
    if (!text || text.trim() === '') {
      throw new Error('API가 빈 응답을 반환했습니다');
    }

    try {
      return JSON.parse(text);
    } catch (parseError) {
      console.error('JSON 파싱 실패. 응답 내용:', text.substring(0, 200));
      throw new Error(`JSON 파싱 실패: ${parseError instanceof Error ? parseError.message : 'Unknown error'}`);
    }
  };

  /**
   * 🛠️ 안전한 fetch 래퍼 함수
   */
  const safeFetch = async (url: string, options?: RequestInit): Promise<any> => {
    try {
      console.log(`🌐 API 호출 시작: ${url}`);
      
      const response = await fetch(url, {
        timeout: 10000, // 10초 타임아웃
        ...options,
        headers: {
          'Accept': 'application/json',
          'Content-Type': 'application/json',
          ...options?.headers,
        },
      });

      console.log(`📡 응답 상태: ${response.status} ${response.statusText}`);

      if (!response.ok) {
        let errorMessage = `HTTP ${response.status}: ${response.statusText}`;
        
        // 에러 응답도 JSON으로 파싱 시도
        try {
          const errorData = await safeJsonParse(response);
          errorMessage = errorData.message || errorData.error || errorMessage;
        } catch {
          // JSON 파싱 실패 시 기본 에러 메시지 사용
        }
        
        throw new Error(errorMessage);
      }

      return await safeJsonParse(response);
    } catch (error) {
      if (error instanceof TypeError && error.message.includes('fetch')) {
        throw new Error('네트워크 연결 실패: 백엔드 서버가 실행 중인지 확인하세요');
      }
      throw error;
    }
  };

  /**
   * 대시보드 개요 데이터 로드 - 강화된 에러 처리
   */
  const loadDashboardOverview = useCallback(async () => {
    try {
      setIsLoading(true);
      setError(null);

      console.log('🎯 대시보드 개요 데이터 로드 시작...');

      // 🛠️ 백엔드 상태 먼저 확인
      try {
        const healthResponse = await safeFetch('/api/health');
        console.log('✅ 백엔드 헬스체크 성공:', healthResponse);
      } catch (healthError) {
        console.warn('⚠️ 백엔드 헬스체크 실패:', healthError);
        throw new Error('백엔드 서버에 연결할 수 없습니다. 서버가 실행 중인지 확인하세요.');
      }

      // 🛠️ 대시보드 API 호출
      const data = await safeFetch('/api/dashboard/overview');

      if (data && data.success && data.data) {
        setDashboardData(data.data);
        console.log('✅ 대시보드 개요 데이터 로드 완료:', data.data);
      } else if (data) {
        // API가 응답했지만 예상된 구조가 아님
        console.warn('⚠️ 예상과 다른 API 응답:', data);
        
        // 🔄 임시 데이터로 폴백
        setDashboardData(createFallbackDashboardData());
        setError('일부 데이터를 불러오지 못했습니다. 임시 데이터를 표시합니다.');
      } else {
        throw new Error('API가 올바른 형식의 데이터를 반환하지 않았습니다');
      }

    } catch (err) {
      console.error('❌ 대시보드 데이터 로드 실패:', err);
      const errorMessage = err instanceof Error ? err.message : '알 수 없는 오류';
      setError(errorMessage);
      
      // 🔄 완전 실패 시 기본 데이터로 폴백
      setDashboardData(createFallbackDashboardData());
    } finally {
      setIsLoading(false);
      setLastUpdate(new Date());
    }
  }, []);

  /**
   * 🔄 폴백 대시보드 데이터 생성 - 실제 서비스들 포함
   */
  const createFallbackDashboardData = (): DashboardData => {
    return {
      services: {
        total: 4,
        running: 1,
        stopped: 3,
        error: 0,
        details: [
          {
            name: 'backend',
            displayName: 'Backend API',
            status: 'running',
            icon: 'fas fa-server',
            controllable: false,
            description: 'Node.js 백엔드 서비스',
            uptime: Math.floor(Math.random() * 3600),
            memory_usage: Math.floor(Math.random() * 100) + 50,
            cpu_usage: Math.floor(Math.random() * 30) + 5
          },
          {
            name: 'collector',
            displayName: 'Data Collector',
            status: 'stopped',
            icon: 'fas fa-download',
            controllable: true,
            description: 'C++ 데이터 수집 서비스'
          },
          {
            name: 'redis',
            displayName: 'Redis Cache',
            status: 'stopped',
            icon: 'fas fa-database',
            controllable: true,
            description: '실시간 데이터 캐시'
          },
          {
            name: 'rabbitmq',
            displayName: 'RabbitMQ',
            status: 'stopped',
            icon: 'fas fa-exchange-alt',
            controllable: true,
            description: '메시지 큐 서비스'
          }
        ]
      },
      system_metrics: {
        dataPointsPerSecond: Math.floor(Math.random() * 100) + 50,
        avgResponseTime: Math.floor(Math.random() * 50) + 10,
        dbQueryTime: Math.floor(Math.random() * 20) + 5,
        cpuUsage: Math.floor(Math.random() * 60) + 20,
        memoryUsage: Math.floor(Math.random() * 40) + 30,
        diskUsage: Math.floor(Math.random() * 30) + 45,
        networkUsage: Math.floor(Math.random() * 50) + 10,
        activeConnections: Math.floor(Math.random() * 20) + 5,
        queueSize: Math.floor(Math.random() * 10)
      },
      device_summary: {
        total_devices: Math.floor(Math.random() * 20) + 5,
        connected_devices: Math.floor(Math.random() * 15) + 3,
        disconnected_devices: Math.floor(Math.random() * 5) + 1,
        error_devices: Math.floor(Math.random() * 3),
        protocols_count: 3,
        sites_count: 2
      },
      alarms: {
        total: Math.floor(Math.random() * 10) + 1,
        unacknowledged: Math.floor(Math.random() * 5),
        critical: Math.floor(Math.random() * 3),
        warnings: Math.floor(Math.random() * 5) + 1,
        recent_alarms: [
          {
            id: 'alarm_1',
            type: 'warning',
            message: '임시 알람: 시스템 데이터를 불러오는 중입니다',
            icon: 'fas fa-exclamation-triangle',
            timestamp: new Date().toISOString(),
            device_id: 1,
            acknowledged: false
          }
        ]
      },
      health_status: {
        overall: 'degraded',
        database: 'warning',
        network: 'healthy',
        storage: 'healthy'
      },
      last_updated: new Date().toISOString()
    };
  };

  /**
   * 서비스 제어 - 강화된 에러 처리
   */
  const handleServiceControl = async (serviceName: string, action: 'start' | 'stop' | 'restart') => {
    try {
      console.log(`🔧 서비스 ${serviceName} ${action} 요청...`);

      const data = await safeFetch(`/api/dashboard/service/${serviceName}/control`, {
        method: 'POST',
        body: JSON.stringify({ action })
      });

      if (data && data.success) {
        console.log(`✅ 서비스 ${serviceName} ${action} 완료`);
        alert(`서비스 ${action} 완료: ${data.data?.message || 'Success'}`);
        
        // 서비스 상태 새로고침
        setTimeout(() => {
          loadDashboardOverview();
        }, 2000);

      } else {
        throw new Error(data?.message || `서비스 ${action} 실패`);
      }
    } catch (err) {
      console.error(`❌ 서비스 ${serviceName} ${action} 실패:`, err);
      alert(`서비스 ${action} 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}`);
    }
  };

  // ============================================================================
  // 🔄 기존 라이프사이클 hooks 그대로 유지
  // ============================================================================

  useEffect(() => {
    loadDashboardOverview();
  }, [loadDashboardOverview]);

  // 자동 새로고침
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      loadDashboardOverview();
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [autoRefresh, refreshInterval, loadDashboardOverview]);

  // ============================================================================
  // 🎨 기존 렌더링 헬퍼 함수들 그대로 유지
  // ============================================================================

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
    if (!seconds || seconds <= 0) return '알 수 없음';
    
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
    if (!mb || mb <= 0) return '알 수 없음';
    
    if (mb >= 1024) {
      return `${(mb / 1024).toFixed(1)}GB`;
    } else {
      return `${mb}MB`;
    }
  };

  // ============================================================================
  // 🎨 원래 의도된 레이아웃: 왼쪽 서비스 + 오른쪽 2x2 상태
  // ============================================================================

  if (isLoading && !dashboardData) {
    return (
      <div className="widget-loading">
        <div className="loading-spinner">
          <i className="fas fa-spinner fa-spin"></i>
          <span>대시보드를 불러오는 중...</span>
        </div>
        <div style={{ marginTop: '1rem', fontSize: '0.9rem', color: '#666' }}>
          백엔드 서버와 통신 중입니다...
        </div>
      </div>
    );
  }

  return (
    <div className="dashboard-container">
      {/* 🎯 기존 CSS 클래스 사용: dashboard-header */}
      <div className="dashboard-header">
        <div>
          <h1 className="dashboard-title">시스템 대시보드</h1>
          <div style={{ fontSize: '0.9rem', color: '#666', marginTop: '0.5rem' }}>
            PulseOne 시스템의 전체 현황을 실시간으로 모니터링합니다
          </div>
        </div>
        <div className="dashboard-actions">
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
            }}
          >
            <i className="fas fa-sync-alt"></i>
            새로고침
          </button>
        </div>
      </div>

      {/* 🚨 에러 표시 - 기존 CSS 호환 */}
      {error && (
        <div className="dashboard-status-bar" style={{ 
          background: '#fef2f2', 
          borderColor: '#fecaca',
          borderLeftWidth: '4px',
          borderLeftColor: '#dc2626'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <i className="fas fa-exclamation-triangle" style={{ color: '#dc2626' }}></i>
            <div>
              <div style={{ fontWeight: '600', color: '#dc2626' }}>연결 문제 감지</div>
              <div style={{ fontSize: '0.9rem', color: '#991b1b' }}>{error}</div>
              <div style={{ fontSize: '0.8rem', color: '#7f1d1d', marginTop: '0.25rem' }}>
                임시 데이터로 대시보드를 표시합니다. 백엔드 서버 상태를 확인하세요.
              </div>
            </div>
          </div>
          <button 
            onClick={() => setError(null)}
            style={{ 
              background: 'none', 
              border: 'none', 
              color: '#dc2626', 
              cursor: 'pointer',
              fontSize: '1.2rem'
            }}
          >
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 🎯 수정된 메인 레이아웃: 왼쪽 서비스 + 오른쪽 상태 (1:1 비율) */}
      <div style={{ 
        display: 'grid', 
        gridTemplateColumns: '1fr 1fr', 
        gap: '24px',
        marginBottom: '24px' 
      }}>
        
        {/* 📋 왼쪽: 서비스 상태 목록 (세로로 길게) */}
        {dashboardData && (
          <div className="dashboard-widget">
            <div className="widget-header">
              <div className="widget-title">
                <div className="widget-icon success">
                  <i className="fas fa-server"></i>
                </div>
                서비스 상태
              </div>
              <div style={{ fontSize: '0.9rem', color: '#666' }}>
                실행중: {dashboardData.services.running} / 전체: {dashboardData.services.total}
              </div>
            </div>
            <div className="widget-content">
              {/* 서비스 목록 - 간단한 카드 형태 */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                {dashboardData.services.details.map((service) => (
                  <div key={service.name} style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    padding: '16px',
                    background: '#f9fafb',
                    borderRadius: '8px',
                    border: '1px solid #e5e7eb'
                  }}>
                    {/* 왼쪽: 서비스 정보 */}
                    <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flex: 1 }}>
                      {/* 상태 점 */}
                      <span className={`status-dot status-dot-${service.status}`}></span>
                      
                      {/* 아이콘 */}
                      <i className={service.icon} style={{ fontSize: '1.5rem', color: '#6b7280', width: '24px' }}></i>
                      
                      {/* 서비스 이름 & 설명 */}
                      <div style={{ flex: 1 }}>
                        <div style={{ fontWeight: '600', color: '#111827', marginBottom: '2px' }}>
                          {service.displayName}
                        </div>
                        <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                          {service.description}
                        </div>
                        
                        {/* 포트 정보 */}
                        {(service.name === 'backend' || service.name === 'redis' || service.name === 'rabbitmq') && (
                          <div style={{ fontSize: '0.7rem', color: '#059669', fontFamily: 'monospace', marginTop: '2px' }}>
                            포트: {service.name === 'backend' ? '3000' :
                                 service.name === 'redis' ? '6379' :
                                 service.name === 'rabbitmq' ? '5672' : '-'}
                          </div>
                        )}
                      </div>
                    </div>

                    {/* 중간: 리소스 정보 */}
                    <div style={{ textAlign: 'right', minWidth: '100px', marginRight: '16px' }}>
                      {service.memory_usage && service.memory_usage > 0 && (
                        <div style={{ fontSize: '0.75rem', color: '#374151' }}>
                          메모리: {formatMemoryUsage(service.memory_usage)}
                        </div>
                      )}
                      {service.cpu_usage && service.cpu_usage > 0 && (
                        <div style={{ fontSize: '0.75rem', color: '#374151' }}>
                          CPU: {service.cpu_usage}%
                        </div>
                      )}
                      {service.uptime && service.uptime > 0 && (
                        <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                          {formatUptime(service.uptime)}
                        </div>
                      )}
                      {service.status === 'running' && (!service.memory_usage && !service.cpu_usage && !service.uptime) && (
                        <div style={{ fontSize: '0.75rem', color: '#059669' }}>실행중</div>
                      )}
                    </div>

                    {/* 오른쪽: 제어 버튼 */}
                    <div style={{ display: 'flex', gap: '6px' }}>
                      {service.controllable ? (
                        service.status === 'running' ? (
                          <>
                            <button 
                              onClick={() => handleServiceControl(service.name, 'stop')}
                              className="btn btn-sm btn-warning"
                              title="중지"
                              style={{ padding: '6px 8px' }}
                            >
                              <i className="fas fa-stop"></i>
                            </button>
                            <button 
                              onClick={() => handleServiceControl(service.name, 'restart')}
                              className="btn btn-sm btn-secondary"
                              title="재시작"
                              style={{ padding: '6px 8px' }}
                            >
                              <i className="fas fa-redo"></i>
                            </button>
                          </>
                        ) : (
                          <button 
                            onClick={() => handleServiceControl(service.name, 'start')}
                            className="btn btn-sm btn-success"
                            title="시작"
                            style={{ padding: '6px 12px' }}
                          >
                            <i className="fas fa-play"></i>
                          </button>
                        )
                      ) : (
                        <span style={{ 
                          fontSize: '0.7rem', 
                          color: '#6b7280',
                          padding: '4px 8px',
                          background: '#f3f4f6',
                          borderRadius: '4px'
                        }}>
                          필수
                        </span>
                      )}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {/* 📊 오른쪽: 시스템 상태 (2x2 그리드) */}
        {dashboardData && (
          <div style={{ display: 'grid', gridTemplateRows: '1fr 1fr', gap: '16px' }}>
            
            {/* 시스템 상태 개요 */}
            <div className="dashboard-widget">
              <div className="widget-header">
                <div className="widget-title">
                  <div className="widget-icon primary">
                    <i className="fas fa-heartbeat"></i>
                  </div>
                  시스템 상태
                </div>
                <span className={`status status-${dashboardData.health_status.overall === 'healthy' ? 'running' : 'paused'}`}>
                  <span className={`status-dot status-dot-${dashboardData.health_status.overall === 'healthy' ? 'running' : 'paused'}`}></span>
                  {dashboardData.health_status.overall === 'healthy' ? '정상' : '주의'}
                </span>
              </div>
              <div className="widget-content">
                <div className="overview-grid">
                  <div className="overview-item">
                    <div className="overview-value">{dashboardData.device_summary.total_devices}</div>
                    <div className="overview-label">전체 디바이스</div>
                    <div className="overview-change positive">
                      연결: {dashboardData.device_summary.connected_devices}
                    </div>
                  </div>
                  <div className="overview-item">
                    <div className="overview-value">{dashboardData.system_metrics.dataPointsPerSecond}</div>
                    <div className="overview-label">데이터 포인트/초</div>
                    <div className="overview-change positive">
                      응답: {dashboardData.system_metrics.avgResponseTime}ms
                    </div>
                  </div>
                  <div className="overview-item">
                    <div className="overview-value">{dashboardData.alarms.total}</div>
                    <div className="overview-label">활성 알람</div>
                    <div className="overview-change negative">
                      심각: {dashboardData.alarms.critical}
                    </div>
                  </div>
                  <div className="overview-item">
                    <div className="overview-value">{dashboardData.services.running}</div>
                    <div className="overview-label">실행 중인 서비스</div>
                    <div className="overview-change positive">
                      전체: {dashboardData.services.total}
                    </div>
                  </div>
                </div>
              </div>
            </div>

            {/* 시스템 리소스 */}
            <div className="dashboard-widget">
              <div className="widget-header">
                <div className="widget-title">
                  <div className="widget-icon warning">
                    <i className="fas fa-chart-bar"></i>
                  </div>
                  시스템 리소스
                </div>
              </div>
              <div className="widget-content">
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                  <div>
                    <div style={{ fontSize: '0.75rem', color: '#6b7280', marginBottom: '4px' }}>CPU 사용률</div>
                    <div style={{ fontSize: '1.25rem', fontWeight: '600', marginBottom: '8px' }}>
                      {dashboardData.system_metrics.cpuUsage}%
                    </div>
                    <div className="metric-bar">
                      <div 
                        className={`metric-fill ${dashboardData.system_metrics.cpuUsage > 80 ? 'high' : 
                                                 dashboardData.system_metrics.cpuUsage > 60 ? 'medium' : 'low'}`}
                        style={{ width: `${dashboardData.system_metrics.cpuUsage}%` }}
                      ></div>
                    </div>
                  </div>
                  
                  <div>
                    <div style={{ fontSize: '0.75rem', color: '#6b7280', marginBottom: '4px' }}>메모리 사용률</div>
                    <div style={{ fontSize: '1.25rem', fontWeight: '600', marginBottom: '8px' }}>
                      {dashboardData.system_metrics.memoryUsage}%
                    </div>
                    <div className="metric-bar">
                      <div 
                        className={`metric-fill ${dashboardData.system_metrics.memoryUsage > 80 ? 'high' : 
                                                 dashboardData.system_metrics.memoryUsage > 60 ? 'medium' : 'low'}`}
                        style={{ width: `${dashboardData.system_metrics.memoryUsage}%` }}
                      ></div>
                    </div>
                  </div>
                  
                  <div>
                    <div style={{ fontSize: '0.75rem', color: '#6b7280', marginBottom: '4px' }}>디스크 사용률</div>
                    <div style={{ fontSize: '1.25rem', fontWeight: '600', marginBottom: '8px' }}>
                      {dashboardData.system_metrics.diskUsage}%
                    </div>
                    <div className="metric-bar">
                      <div 
                        className={`metric-fill ${dashboardData.system_metrics.diskUsage > 80 ? 'high' : 
                                                 dashboardData.system_metrics.diskUsage > 60 ? 'medium' : 'low'}`}
                        style={{ width: `${dashboardData.system_metrics.diskUsage}%` }}
                      ></div>
                    </div>
                  </div>
                  
                  <div>
                    <div style={{ fontSize: '0.75rem', color: '#6b7280', marginBottom: '4px' }}>네트워크 사용률</div>
                    <div style={{ fontSize: '1.25rem', fontWeight: '600', marginBottom: '8px' }}>
                      {dashboardData.system_metrics.networkUsage} Mbps
                    </div>
                    <div className="metric-bar">
                      <div 
                        className="metric-fill low"
                        style={{ width: `${Math.min(dashboardData.system_metrics.networkUsage, 100)}%` }}
                      ></div>
                    </div>
                  </div>
                </div>
              </div>
            </div>

          </div>
        )}
      </div>

      {/* 📊 하단: 알람 위젯 (전체 폭) */}
      {dashboardData && dashboardData.alarms.recent_alarms.length > 0 && (
        <div className="dashboard-widget alarm-widget">
          <div className="widget-header">
            <div className="widget-title">
              <div className="widget-icon error">
                <i className="fas fa-bell"></i>
              </div>
              최근 알람
            </div>
            <a href="#/alarms/active" className="btn btn-sm btn-outline">
              모든 알람 보기 <i className="fas fa-arrow-right"></i>
            </a>
          </div>
          <div className="widget-content">
            <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
              {dashboardData.alarms.recent_alarms.map((alarm) => (
                <div key={alarm.id} style={{
                  display: 'flex',
                  alignItems: 'flex-start',
                  gap: '12px',
                  padding: '12px',
                  borderRadius: '6px',
                  background: '#f9fafb',
                  border: '1px solid #e5e7eb'
                }}>
                  {/* 알람 아이콘 */}
                  <div style={{ marginTop: '2px' }}>
                    <i className={getAlarmTypeIcon(alarm.type)} style={{ fontSize: '1.1rem' }}></i>
                  </div>
                  
                  {/* 알람 내용 */}
                  <div style={{ flex: 1 }}>
                    <div style={{ fontWeight: '500', color: '#111827', marginBottom: '4px' }}>
                      {alarm.message}
                    </div>
                    <div style={{ fontSize: '0.75rem', color: '#6b7280' }}>
                      Device {alarm.device_id} • {new Date(alarm.timestamp).toLocaleString()}
                      {alarm.acknowledged && (
                        <span style={{ color: '#22c55e', marginLeft: '8px' }}>
                          <i className="fas fa-check"></i> 확인됨
                        </span>
                      )}
                    </div>
                  </div>
                  
                  {/* 시간 */}
                  <div style={{ fontSize: '0.75rem', color: '#9ca3af', whiteSpace: 'nowrap' }}>
                    {new Date(alarm.timestamp).toLocaleTimeString()}
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* 상태 바 - 기존 CSS 클래스 사용 */}
      <div className="dashboard-status-bar">
        <div className="dashboard-controls">
          <span>마지막 업데이트: {lastUpdate.toLocaleTimeString()}</span>
          {autoRefresh && (
            <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
              <div className="live-indicator"></div>
              {refreshInterval / 1000}초마다 자동 새로고침
            </span>
          )}
          {dashboardData && (
            <span className={`status status-${dashboardData.health_status.overall === 'healthy' ? 'running' : 'paused'}`}>
              <span className={`status-dot status-dot-${dashboardData.health_status.overall === 'healthy' ? 'running' : 'paused'}`}></span>
              시스템 상태: {dashboardData.health_status.overall === 'healthy' ? '정상' : 
                         dashboardData.health_status.overall === 'degraded' ? '주의' : '심각'}
            </span>
          )}
          {error && (
            <span className="status status-error">
              <span className="status-dot status-dot-error"></span>
              연결 문제 - 임시 데이터 표시 중
            </span>
          )}
        </div>
      </div>
    </div>
  );
};

export default Dashboard;