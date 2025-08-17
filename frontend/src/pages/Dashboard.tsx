// ============================================================================
// frontend/src/pages/Dashboard.tsx
// 📊 PulseOne 시스템 대시보드 - 완성본
// 원래 의도된 레이아웃: 왼쪽 서비스 목록 + 오른쪽 2x2 시스템 상태
// ============================================================================

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { ApiService } from '../api';
import '../styles/base.css';
import '../styles/dashboard.css';

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
  
  // 실시간 업데이트 설정
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(10000); // 10초
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [consecutiveErrors, setConsecutiveErrors] = useState(0);
  
  // 서비스 제어 상태
  const [controllingServices, setControllingServices] = useState<Set<string>>(new Set());
  
  // 참조
  const refreshTimeoutRef = useRef<NodeJS.Timeout | null>(null);
  const abortControllerRef = useRef<AbortController | null>(null);

  // ==========================================================================
  // 🛠️ 유틸리티 함수들
  // ==========================================================================

  /**
   * 안전한 JSON 파싱
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
   * 안전한 fetch 래퍼
   */
  const safeFetch = async (url: string, options?: RequestInit): Promise<any> => {
    // 기존 요청 취소
    if (abortControllerRef.current) {
      abortControllerRef.current.abort();
    }
    
    abortControllerRef.current = new AbortController();
    
    try {
      console.log(`🌐 API 호출: ${url}`);
      
      const response = await fetch(url, {
        timeout: 15000, // 15초 타임아웃
        signal: abortControllerRef.current.signal,
        ...options,
        headers: {
          'Accept': 'application/json',
          'Content-Type': 'application/json',
          ...options?.headers,
        },
      });

      console.log(`📡 응답: ${response.status} ${response.statusText}`);

      if (!response.ok) {
        let errorMessage = `HTTP ${response.status}: ${response.statusText}`;
        
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
      if (error instanceof Error) {
        if (error.name === 'AbortError') {
          throw new Error('요청이 취소되었습니다');
        }
        if (error.message.includes('fetch')) {
          throw new Error('네트워크 연결 실패: 백엔드 서버 상태를 확인하세요');
        }
      }
      throw error;
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
            icon: 'fas fa-server',
            controllable: false,
            description: 'Node.js 백엔드 서비스',
            port: 3000,
            version: '2.1.0',
            uptime: Math.floor(Math.random() * 3600) + 300,
            memory_usage: Math.floor(Math.random() * 100) + 50,
            cpu_usage: Math.floor(Math.random() * 30) + 5
          },
          {
            name: 'collector',
            displayName: 'Data Collector',
            status: 'stopped',
            icon: 'fas fa-download',
            controllable: true,
            description: 'C++ 데이터 수집 서비스',
            port: 8080,
            last_error: 'Binary not found'
          },
          {
            name: 'redis',
            displayName: 'Redis Cache',
            status: 'stopped',
            icon: 'fas fa-database',
            controllable: true,
            description: '실시간 데이터 캐시',
            port: 6379,
            last_error: 'Service not installed'
          },
          {
            name: 'rabbitmq',
            displayName: 'RabbitMQ',
            status: 'stopped',
            icon: 'fas fa-exchange-alt',
            controllable: true,
            description: '메시지 큐 서비스',
            port: 5672
          },
          {
            name: 'postgresql',
            displayName: 'PostgreSQL',
            status: 'stopped',
            icon: 'fas fa-elephant',
            controllable: true,
            description: '메타데이터 저장소',
            port: 5432
          }
        ]
      },
      system_metrics: {
        dataPointsPerSecond: Math.floor(Math.random() * 150) + 50,
        avgResponseTime: Math.floor(Math.random() * 100) + 20,
        dbQueryTime: Math.floor(Math.random() * 50) + 5,
        cpuUsage: Math.floor(Math.random() * 70) + 10,
        memoryUsage: Math.floor(Math.random() * 60) + 20,
        diskUsage: Math.floor(Math.random() * 40) + 30,
        networkUsage: Math.floor(Math.random() * 80) + 10,
        activeConnections: Math.floor(Math.random() * 30) + 5,
        queueSize: Math.floor(Math.random() * 20),
        timestamp: now.toISOString()
      },
      device_summary: {
        total_devices: Math.floor(Math.random() * 25) + 5,
        connected_devices: Math.floor(Math.random() * 15) + 2,
        disconnected_devices: Math.floor(Math.random() * 8) + 1,
        error_devices: Math.floor(Math.random() * 3),
        protocols_count: 3,
        sites_count: 2,
        data_points_count: Math.floor(Math.random() * 200) + 50,
        enabled_devices: Math.floor(Math.random() * 20) + 3
      },
      alarms: {
        total: Math.floor(Math.random() * 15) + 1,
        unacknowledged: Math.floor(Math.random() * 8),
        critical: Math.floor(Math.random() * 3),
        warnings: Math.floor(Math.random() * 8) + 1,
        recent_24h: Math.floor(Math.random() * 25) + 5,
        recent_alarms: [
          {
            id: 'fallback_alarm_1',
            type: 'warning',
            message: '백엔드 연결 실패 - 시뮬레이션 데이터를 표시합니다',
            icon: 'fas fa-exclamation-triangle',
            timestamp: now.toISOString(),
            device_id: 1,
            device_name: 'Backend Server',
            acknowledged: false,
            severity: 'medium'
          },
          {
            id: 'fallback_alarm_2',
            type: 'info',
            message: '데이터베이스 연결이 복원되면 실제 데이터가 표시됩니다',
            icon: 'fas fa-info-circle',
            timestamp: new Date(now.getTime() - 300000).toISOString(), // 5분 전
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
        api_response_time: Math.floor(Math.random() * 200) + 50,
        database_response_time: Math.floor(Math.random() * 100) + 10,
        cache_hit_rate: Math.floor(Math.random() * 30) + 60,
        error_rate: Math.random() * 5,
        throughput_per_second: Math.floor(Math.random() * 500) + 100
      },
      last_updated: now.toISOString()
    };
  };

  // ==========================================================================
  // 🔄 데이터 로드 함수들
  // ==========================================================================

  /**
   * 대시보드 개요 데이터 로드
   */
  const loadDashboardOverview = useCallback(async (showLoading = false) => {
    try {
      if (showLoading) {
        setIsLoading(true);
      }
      setError(null);

      console.log('🎯 대시보드 데이터 로드 시작...');

      // 백엔드 헬스체크 먼저 시도
      let healthData;
      try {
        healthData = await safeFetch('/api/health');
        console.log('✅ 헬스체크 성공');
        setConnectionStatus('connected');
        setConsecutiveErrors(0);
      } catch (healthError) {
        console.warn('⚠️ 헬스체크 실패:', healthError);
        setConnectionStatus('disconnected');
        setConsecutiveErrors(prev => prev + 1);
        throw new Error('백엔드 서버에 연결할 수 없습니다');
      }

      // 대시보드 데이터 로드
      let dashboardResponse;
      try {
        dashboardResponse = await safeFetch('/api/dashboard/overview');
      } catch (dashboardError) {
        // 헬스체크는 성공했지만 대시보드 API 실패
        console.warn('⚠️ 대시보드 API 실패, 기본 구조 사용:', dashboardError);
        dashboardResponse = { success: false, error: dashboardError.message };
      }

      // 서비스 상태 추가 로드 시도
      let servicesData;
      try {
        servicesData = await safeFetch('/api/dashboard/services/status');
        console.log('✅ 서비스 상태 로드 성공');
      } catch (servicesError) {
        console.warn('⚠️ 서비스 상태 로드 실패:', servicesError);
      }

      // 데이터 통합 및 검증
      if (dashboardResponse?.success && dashboardResponse?.data) {
        // 서비스 데이터 병합
        if (servicesData?.success && servicesData?.data) {
          dashboardResponse.data.services = {
            ...dashboardResponse.data.services,
            ...servicesData.data
          };
        }
        
        setDashboardData(dashboardResponse.data);
        console.log('✅ 대시보드 데이터 로드 완료');
      } else {
        console.warn('⚠️ API 응답 형식 불일치, 폴백 데이터 사용');
        setDashboardData(createFallbackDashboardData());
        setError('일부 데이터를 불러올 수 없어 시뮬레이션 데이터를 표시합니다');
      }

    } catch (err) {
      console.error('❌ 대시보드 로드 실패:', err);
      const errorMessage = err instanceof Error ? err.message : '알 수 없는 오류';
      setError(errorMessage);
      setConnectionStatus('disconnected');
      setConsecutiveErrors(prev => prev + 1);
      
      // 폴백 데이터 설정
      setDashboardData(createFallbackDashboardData());
    } finally {
      setIsLoading(false);
      setLastUpdate(new Date());
    }
  }, []);

  /**
   * 서비스 제어
   */
  const handleServiceControl = async (serviceName: string, action: 'start' | 'stop' | 'restart') => {
    if (controllingServices.has(serviceName)) {
      console.warn(`⚠️ 서비스 ${serviceName}이 이미 제어 중입니다`);
      return;
    }

    try {
      setControllingServices(prev => new Set(prev).add(serviceName));
      console.log(`🔧 서비스 ${serviceName} ${action} 시작...`);

      // 서비스 상태를 즉시 업데이트 (낙관적 업데이트)
      if (dashboardData) {
        const updatedData = { ...dashboardData };
        const service = updatedData.services.details.find(s => s.name === serviceName);
        if (service) {
          service.status = action === 'start' ? 'starting' : action === 'stop' ? 'stopping' : 'starting';
        }
        setDashboardData(updatedData);
      }

      const response = await safeFetch(`/api/dashboard/service/${serviceName}/control`, {
        method: 'POST',
        body: JSON.stringify({ action })
      });

      if (response?.success) {
        console.log(`✅ 서비스 ${serviceName} ${action} 완료`);
        
        // 성공 알림
        const notification = document.createElement('div');
        notification.className = 'notification success';
        notification.innerHTML = `
          <i class="fas fa-check-circle"></i>
          서비스 ${action} 완료: ${response.data?.message || 'Success'}
        `;
        document.body.appendChild(notification);
        
        setTimeout(() => {
          notification.remove();
        }, 3000);

        // 2초 후 상태 새로고침
        setTimeout(() => {
          loadDashboardOverview();
        }, 2000);

      } else {
        throw new Error(response?.message || `서비스 ${action} 실패`);
      }
    } catch (err) {
      console.error(`❌ 서비스 ${serviceName} ${action} 실패:`, err);
      
      // 에러 알림
      const notification = document.createElement('div');
      notification.className = 'notification error';
      notification.innerHTML = `
        <i class="fas fa-exclamation-circle"></i>
        서비스 ${action} 실패: ${err instanceof Error ? err.message : '알 수 없는 오류'}
      `;
      document.body.appendChild(notification);
      
      setTimeout(() => {
        notification.remove();
      }, 5000);

      // 상태 복원
      loadDashboardOverview();
    } finally {
      setControllingServices(prev => {
        const next = new Set(prev);
        next.delete(serviceName);
        return next;
      });
    }
  };

  // ==========================================================================
  // 🎨 렌더링 헬퍼 함수들
  // ==========================================================================

  const getServiceStatusIcon = (status: string) => {
    switch (status) {
      case 'running': return 'fas fa-check-circle text-success';
      case 'stopped': return 'fas fa-stop-circle text-muted';
      case 'error': return 'fas fa-times-circle text-danger';
      case 'starting': return 'fas fa-spinner fa-spin text-info';
      case 'stopping': return 'fas fa-spinner fa-spin text-warning';
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
      case 'critical': return 'fas fa-exclamation-triangle text-danger';
      case 'major': return 'fas fa-exclamation-circle text-danger';
      case 'minor': return 'fas fa-exclamation-circle text-warning';
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

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  const getMetricBarClass = (value: number, type: 'cpu' | 'memory' | 'disk' | 'network' = 'cpu') => {
    switch (type) {
      case 'cpu':
      case 'memory':
        return value > 80 ? 'high' : value > 60 ? 'medium' : 'low';
      case 'disk':
        return value > 90 ? 'high' : value > 75 ? 'medium' : 'low';
      case 'network':
        return value > 100 ? 'high' : value > 50 ? 'medium' : 'low';
      default:
        return 'low';
    }
  };

  // ==========================================================================
  // 🔄 라이프사이클 이벤트
  // ==========================================================================

  useEffect(() => {
    loadDashboardOverview(true);
    
    // 컴포넌트 언마운트 시 정리
    return () => {
      if (refreshTimeoutRef.current) {
        clearTimeout(refreshTimeoutRef.current);
      }
      if (abortControllerRef.current) {
        abortControllerRef.current.abort();
      }
    };
  }, [loadDashboardOverview]);

  // 자동 새로고침
  useEffect(() => {
    if (!autoRefresh) {
      if (refreshTimeoutRef.current) {
        clearTimeout(refreshTimeoutRef.current);
      }
      return;
    }

    const scheduleNextRefresh = () => {
      refreshTimeoutRef.current = setTimeout(() => {
        loadDashboardOverview();
        scheduleNextRefresh();
      }, refreshInterval);
    };

    scheduleNextRefresh();

    return () => {
      if (refreshTimeoutRef.current) {
        clearTimeout(refreshTimeoutRef.current);
      }
    };
  }, [autoRefresh, refreshInterval, loadDashboardOverview]);

  // 연속 에러 시 자동 새로고침 중지
  useEffect(() => {
    if (consecutiveErrors >= 5) {
      setAutoRefresh(false);
      console.warn('⚠️ 연속 에러 5회 초과, 자동 새로고침 중지');
    }
  }, [consecutiveErrors]);

  // ==========================================================================
  // 🎨 컴포넌트 렌더링
  // ==========================================================================

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
        <div style={{ marginTop: '0.5rem', fontSize: '0.8rem', color: '#999' }}>
          {connectionStatus === 'reconnecting' && '재연결 시도 중...'}
          {connectionStatus === 'disconnected' && '연결 대기 중...'}
        </div>
      </div>
    );
  }

  return (
    <div className="dashboard-container">
      {/* 📊 대시보드 헤더 */}
      <div className="dashboard-header">
        <div>
          <h1 className="dashboard-title">
            시스템 대시보드
            <span className={`connection-indicator ${connectionStatus}`}>
              <span className={`status-dot status-dot-${connectionStatus === 'connected' ? 'running' : 'error'}`}></span>
              {connectionStatus === 'connected' ? '연결됨' : 
               connectionStatus === 'reconnecting' ? '재연결 중' : '연결 끊김'}
            </span>
          </h1>
          <div className="dashboard-subtitle">
            PulseOne 시스템의 전체 현황을 실시간으로 모니터링합니다
            {consecutiveErrors > 0 && (
              <span style={{ color: '#dc2626', marginLeft: '8px' }}>
                (에러 {consecutiveErrors}회)
              </span>
            )}
          </div>
        </div>
        <div className="dashboard-actions">
          <button 
            className={`btn btn-secondary ${autoRefresh ? 'active' : ''}`}
            onClick={() => setAutoRefresh(!autoRefresh)}
            title={autoRefresh ? '자동새로고침 중지' : '자동새로고침 시작'}
          >
            <i className={`fas fa-${autoRefresh ? 'pause' : 'play'}`}></i>
            {autoRefresh ? '새로고침 중' : '일시정지'}
          </button>
          <select 
            value={refreshInterval} 
            onChange={(e) => setRefreshInterval(Number(e.target.value))}
            className="refresh-interval-select"
            disabled={!autoRefresh}
          >
            <option value={5000}>5초</option>
            <option value={10000}>10초</option>
            <option value={30000}>30초</option>
            <option value={60000}>1분</option>
          </select>
          <button 
            className="btn btn-primary"
            onClick={() => loadDashboardOverview(true)}
            disabled={isLoading}
          >
            <i className={`fas fa-sync-alt ${isLoading ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
        </div>
      </div>

      {/* 🚨 에러 및 경고 표시 */}
      {error && (
        <div className="dashboard-alert error">
          <div className="alert-content">
            <i className="fas fa-exclamation-triangle"></i>
            <div>
              <div className="alert-title">연결 문제 감지</div>
              <div className="alert-message">{error}</div>
              <div className="alert-help">
                임시 데이터로 대시보드를 표시합니다. 백엔드 서버(localhost:3000) 상태를 확인하세요.
              </div>
            </div>
          </div>
          <button onClick={() => setError(null)} className="alert-close">
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 📊 메인 대시보드 그리드 */}
      <div className="dashboard-main-grid">
        
        {/* 📋 왼쪽: 서비스 상태 목록 */}
        {dashboardData && (
          <div className="dashboard-widget services-widget">
            <div className="widget-header">
              <div className="widget-title">
                <div className="widget-icon success">
                  <i className="fas fa-server"></i>
                </div>
                서비스 상태
              </div>
              <div className="widget-summary">
                <span className="summary-item success">
                  <span className="summary-count">{dashboardData.services.running}</span>
                  실행중
                </span>
                <span className="summary-item warning">
                  <span className="summary-count">{dashboardData.services.stopped}</span>
                  중지됨
                </span>
                {dashboardData.services.error > 0 && (
                  <span className="summary-item error">
                    <span className="summary-count">{dashboardData.services.error}</span>
                    오류
                  </span>
                )}
              </div>
            </div>
            <div className="widget-content">
              <div className="services-list">
                {dashboardData.services.details.map((service) => (
                  <div key={service.name} className={`service-item ${service.status}`}>
                    <div className="service-main">
                      {/* 상태 표시 */}
                      <div className="service-status">
                        <span className={`status-dot status-dot-${service.status}`}></span>
                      </div>
                      
                      {/* 서비스 정보 */}
                      <div className="service-icon">
                        <i className={service.icon}></i>
                      </div>
                      
                      <div className="service-info">
                        <div className="service-name">{service.displayName}</div>
                        <div className="service-description">{service.description}</div>
                        {service.port && (
                          <div className="service-port">포트: {service.port}</div>
                        )}
                        {service.version && (
                          <div className="service-version">v{service.version}</div>
                        )}
                        {service.last_error && service.status === 'error' && (
                          <div className="service-error">{service.last_error}</div>
                        )}
                      </div>
                    </div>

                    <div className="service-metrics">
                      {service.memory_usage && service.memory_usage > 0 && (
                        <div className="metric">
                          <span className="metric-label">메모리</span>
                          <span className="metric-value">{formatMemoryUsage(service.memory_usage)}</span>
                        </div>
                      )}
                      {service.cpu_usage && service.cpu_usage > 0 && (
                        <div className="metric">
                          <span className="metric-label">CPU</span>
                          <span className="metric-value">{service.cpu_usage}%</span>
                        </div>
                      )}
                      {service.uptime && service.uptime > 0 && (
                        <div className="metric">
                          <span className="metric-label">가동시간</span>
                          <span className="metric-value">{formatUptime(service.uptime)}</span>
                        </div>
                      )}
                    </div>

                    <div className="service-controls">
                      {service.controllable ? (
                        <div className="control-buttons">
                          {service.status === 'running' ? (
                            <>
                              <button 
                                onClick={() => handleServiceControl(service.name, 'stop')}
                                disabled={controllingServices.has(service.name)}
                                className="btn btn-sm btn-warning"
                                title="중지"
                              >
                                <i className="fas fa-stop"></i>
                              </button>
                              <button 
                                onClick={() => handleServiceControl(service.name, 'restart')}
                                disabled={controllingServices.has(service.name)}
                                className="btn btn-sm btn-secondary"
                                title="재시작"
                              >
                                <i className="fas fa-redo"></i>
                              </button>
                            </>
                          ) : service.status === 'starting' || service.status === 'stopping' ? (
                            <button disabled className="btn btn-sm btn-secondary">
                              <i className="fas fa-spinner fa-spin"></i>
                            </button>
                          ) : (
                            <button 
                              onClick={() => handleServiceControl(service.name, 'start')}
                              disabled={controllingServices.has(service.name)}
                              className="btn btn-sm btn-success"
                              title="시작"
                            >
                              <i className="fas fa-play"></i>
                            </button>
                          )}
                        </div>
                      ) : (
                        <span className="service-badge required">필수</span>
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
          <div className="dashboard-right-panel">
            
            {/* 시스템 개요 */}
            <div className="dashboard-widget overview-widget">
              <div className="widget-header">
                <div className="widget-title">
                  <div className="widget-icon primary">
                    <i className="fas fa-heartbeat"></i>
                  </div>
                  시스템 개요
                </div>
                <span className={`health-status ${dashboardData.health_status.overall}`}>
                  <span className={`status-dot status-dot-${dashboardData.health_status.overall === 'healthy' ? 'running' : 'warning'}`}></span>
                  {dashboardData.health_status.overall === 'healthy' ? '정상' : 
                   dashboardData.health_status.overall === 'degraded' ? '주의' : '심각'}
                </span>
              </div>
              <div className="widget-content">
                <div className="overview-grid">
                  <div className="overview-item">
                    <div className="overview-icon">
                      <i className="fas fa-network-wired"></i>
                    </div>
                    <div className="overview-data">
                      <div className="overview-value">{dashboardData.device_summary.total_devices}</div>
                      <div className="overview-label">디바이스</div>
                      <div className="overview-detail">
                        연결: {dashboardData.device_summary.connected_devices} / 
                        활성: {dashboardData.device_summary.enabled_devices}
                      </div>
                    </div>
                  </div>
                  
                  <div className="overview-item">
                    <div className="overview-icon">
                      <i className="fas fa-tachometer-alt"></i>
                    </div>
                    <div className="overview-data">
                      <div className="overview-value">{dashboardData.system_metrics.dataPointsPerSecond}</div>
                      <div className="overview-label">데이터 포인트/초</div>
                      <div className="overview-detail">
                        응답시간: {dashboardData.system_metrics.avgResponseTime}ms
                      </div>
                    </div>
                  </div>
                  
                  <div className="overview-item">
                    <div className="overview-icon">
                      <i className="fas fa-exclamation-triangle"></i>
                    </div>
                    <div className="overview-data">
                      <div className="overview-value">{dashboardData.alarms.total}</div>
                      <div className="overview-label">활성 알람</div>
                      <div className="overview-detail">
                        심각: {dashboardData.alarms.critical} / 
                        미확인: {dashboardData.alarms.unacknowledged}
                      </div>
                    </div>
                  </div>
                  
                  <div className="overview-item">
                    <div className="overview-icon">
                      <i className="fas fa-database"></i>
                    </div>
                    <div className="overview-data">
                      <div className="overview-value">{dashboardData.device_summary.data_points_count}</div>
                      <div className="overview-label">데이터 포인트</div>
                      <div className="overview-detail">
                        프로토콜: {dashboardData.device_summary.protocols_count}개
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>

            {/* 시스템 리소스 */}
            <div className="dashboard-widget resources-widget">
              <div className="widget-header">
                <div className="widget-title">
                  <div className="widget-icon warning">
                    <i className="fas fa-chart-bar"></i>
                  </div>
                  시스템 리소스
                </div>
                <div className="resource-summary">
                  평균 응답시간: {dashboardData.system_metrics.avgResponseTime}ms
                </div>
              </div>
              <div className="widget-content">
                <div className="resources-grid">
                  <div className="resource-item">
                    <div className="resource-header">
                      <span className="resource-label">CPU 사용률</span>
                      <span className="resource-value">{dashboardData.system_metrics.cpuUsage}%</span>
                    </div>
                    <div className="metric-bar">
                      <div 
                        className={`metric-fill ${getMetricBarClass(dashboardData.system_metrics.cpuUsage, 'cpu')}`}
                        style={{ width: `${Math.min(dashboardData.system_metrics.cpuUsage, 100)}%` }}
                      ></div>
                    </div>
                  </div>
                  
                  <div className="resource-item">
                    <div className="resource-header">
                      <span className="resource-label">메모리</span>
                      <span className="resource-value">{dashboardData.system_metrics.memoryUsage}%</span>
                    </div>
                    <div className="metric-bar">
                      <div 
                        className={`metric-fill ${getMetricBarClass(dashboardData.system_metrics.memoryUsage, 'memory')}`}
                        style={{ width: `${Math.min(dashboardData.system_metrics.memoryUsage, 100)}%` }}
                      ></div>
                    </div>
                  </div>
                  
                  <div className="resource-item">
                    <div className="resource-header">
                      <span className="resource-label">디스크</span>
                      <span className="resource-value">{dashboardData.system_metrics.diskUsage}%</span>
                    </div>
                    <div className="metric-bar">
                      <div 
                        className={`metric-fill ${getMetricBarClass(dashboardData.system_metrics.diskUsage, 'disk')}`}
                        style={{ width: `${Math.min(dashboardData.system_metrics.diskUsage, 100)}%` }}
                      ></div>
                    </div>
                  </div>
                  
                  <div className="resource-item">
                    <div className="resource-header">
                      <span className="resource-label">네트워크</span>
                      <span className="resource-value">{dashboardData.system_metrics.networkUsage} Mbps</span>
                    </div>
                    <div className="metric-bar">
                      <div 
                        className={`metric-fill ${getMetricBarClass(dashboardData.system_metrics.networkUsage, 'network')}`}
                        style={{ width: `${Math.min(dashboardData.system_metrics.networkUsage, 100)}%` }}
                      ></div>
                    </div>
                  </div>
                </div>
                
                <div className="additional-metrics">
                  <div className="metric-row">
                    <span>활성 연결:</span>
                    <span>{dashboardData.system_metrics.activeConnections}</span>
                  </div>
                  <div className="metric-row">
                    <span>큐 크기:</span>
                    <span>{dashboardData.system_metrics.queueSize}</span>
                  </div>
                  <div className="metric-row">
                    <span>DB 쿼리 시간:</span>
                    <span>{dashboardData.system_metrics.dbQueryTime}ms</span>
                  </div>
                </div>
              </div>
            </div>

            {/* 성능 지표 */}
            <div className="dashboard-widget performance-widget">
              <div className="widget-header">
                <div className="widget-title">
                  <div className="widget-icon info">
                    <i className="fas fa-chart-line"></i>
                  </div>
                  성능 지표
                </div>
              </div>
              <div className="widget-content">
                <div className="performance-metrics">
                  <div className="performance-item">
                    <div className="performance-label">API 응답시간</div>
                    <div className="performance-value">{dashboardData.performance.api_response_time}ms</div>
                  </div>
                  <div className="performance-item">
                    <div className="performance-label">DB 응답시간</div>
                    <div className="performance-value">{dashboardData.performance.database_response_time}ms</div>
                  </div>
                  <div className="performance-item">
                    <div className="performance-label">캐시 적중률</div>
                    <div className="performance-value">{dashboardData.performance.cache_hit_rate}%</div>
                  </div>
                  <div className="performance-item">
                    <div className="performance-label">처리량/초</div>
                    <div className="performance-value">{dashboardData.performance.throughput_per_second}</div>
                  </div>
                  <div className="performance-item">
                    <div className="performance-label">에러율</div>
                    <div className="performance-value">{dashboardData.performance.error_rate.toFixed(2)}%</div>
                  </div>
                </div>
              </div>
            </div>

            {/* 헬스 체크 */}
            <div className="dashboard-widget health-widget">
              <div className="widget-header">
                <div className="widget-title">
                  <div className="widget-icon success">
                    <i className="fas fa-heartbeat"></i>
                  </div>
                  헬스 체크
                </div>
              </div>
              <div className="widget-content">
                <div className="health-items">
                  <div className="health-item">
                    <span className="health-label">데이터베이스</span>
                    <span className={`health-status ${dashboardData.health_status.database}`}>
                      <span className={`status-dot status-dot-${dashboardData.health_status.database === 'healthy' ? 'running' : 'warning'}`}></span>
                      {dashboardData.health_status.database}
                    </span>
                  </div>
                  <div className="health-item">
                    <span className="health-label">네트워크</span>
                    <span className={`health-status ${dashboardData.health_status.network}`}>
                      <span className={`status-dot status-dot-${dashboardData.health_status.network === 'healthy' ? 'running' : 'warning'}`}></span>
                      {dashboardData.health_status.network}
                    </span>
                  </div>
                  <div className="health-item">
                    <span className="health-label">스토리지</span>
                    <span className={`health-status ${dashboardData.health_status.storage}`}>
                      <span className={`status-dot status-dot-${dashboardData.health_status.storage === 'healthy' ? 'running' : 'warning'}`}></span>
                      {dashboardData.health_status.storage}
                    </span>
                  </div>
                  <div className="health-item">
                    <span className="health-label">캐시</span>
                    <span className={`health-status ${dashboardData.health_status.cache}`}>
                      <span className={`status-dot status-dot-${dashboardData.health_status.cache === 'healthy' ? 'running' : 'error'}`}></span>
                      {dashboardData.health_status.cache}
                    </span>
                  </div>
                  <div className="health-item">
                    <span className="health-label">메시지 큐</span>
                    <span className={`health-status ${dashboardData.health_status.message_queue}`}>
                      <span className={`status-dot status-dot-${dashboardData.health_status.message_queue === 'healthy' ? 'running' : 'warning'}`}></span>
                      {dashboardData.health_status.message_queue}
                    </span>
                  </div>
                </div>
              </div>
            </div>

          </div>
        )}
      </div>

      {/* 📊 하단: 최근 알람 */}
      {dashboardData && dashboardData.alarms.recent_alarms.length > 0 && (
        <div className="dashboard-widget alarms-widget">
          <div className="widget-header">
            <div className="widget-title">
              <div className="widget-icon error">
                <i className="fas fa-bell"></i>
              </div>
              최근 알람
              <span className="alarm-count">
                24시간 내: {dashboardData.alarms.recent_24h}건
              </span>
            </div>
            <a href="#/alarms/active" className="btn btn-sm btn-outline">
              모든 알람 보기 <i className="fas fa-arrow-right"></i>
            </a>
          </div>
          <div className="widget-content">
            <div className="alarms-list">
              {dashboardData.alarms.recent_alarms.slice(0, 5).map((alarm) => (
                <div key={alarm.id} className={`alarm-item ${alarm.type} ${alarm.acknowledged ? 'acknowledged' : ''}`}>
                  <div className="alarm-icon">
                    <i className={getAlarmTypeIcon(alarm.type)}></i>
                  </div>
                  
                  <div className="alarm-content">
                    <div className="alarm-message">{alarm.message}</div>
                    <div className="alarm-details">
                      {alarm.device_name || `Device ${alarm.device_id}`} • 
                      {new Date(alarm.timestamp).toLocaleString()} •
                      심각도: {alarm.severity}
                      {alarm.acknowledged && alarm.acknowledged_by && (
                        <span className="acknowledged-by">
                          • {alarm.acknowledged_by}가 확인함
                        </span>
                      )}
                    </div>
                  </div>
                  
                  <div className="alarm-meta">
                    <div className="alarm-time">
                      {new Date(alarm.timestamp).toLocaleTimeString()}
                    </div>
                    {alarm.acknowledged ? (
                      <span className="alarm-status acknowledged">
                        <i className="fas fa-check"></i> 확인됨
                      </span>
                    ) : (
                      <span className="alarm-status unacknowledged">
                        <i className="fas fa-exclamation"></i> 미확인
                      </span>
                    )}
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* 📊 대시보드 상태 바 */}
      <div className="dashboard-status-bar">
        <div className="status-left">
          <span className="last-update">
            마지막 업데이트: {lastUpdate.toLocaleTimeString()}
          </span>
          {autoRefresh && (
            <span className="auto-refresh">
              <div className="live-indicator"></div>
              {refreshInterval / 1000}초마다 자동 새로고침
            </span>
          )}
        </div>
        
        <div className="status-right">
          {dashboardData && (
            <span className={`system-status ${dashboardData.health_status.overall}`}>
              <span className={`status-dot status-dot-${dashboardData.health_status.overall === 'healthy' ? 'running' : 'warning'}`}></span>
              시스템: {dashboardData.health_status.overall === 'healthy' ? '정상' : 
                      dashboardData.health_status.overall === 'degraded' ? '주의' : '심각'}
            </span>
          )}
          
          <span className={`connection-status ${connectionStatus}`}>
            <span className={`status-dot status-dot-${connectionStatus === 'connected' ? 'running' : 'error'}`}></span>
            {connectionStatus === 'connected' ? '연결됨' : 
             connectionStatus === 'reconnecting' ? '재연결중' : '연결 끊김'}
          </span>
          
          {error && (
            <span className="error-status">
              <span className="status-dot status-dot-error"></span>
              임시 데이터 표시 중
            </span>
          )}
        </div>
      </div>
    </div>
  );
};

export default Dashboard;