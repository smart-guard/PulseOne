// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceRtuMonitorTab.tsx
// RTU 통신 모니터링 탭 컴포넌트 - protocol_id 지원
// ============================================================================

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { DeviceApiService, Device } from '../../../api/services/deviceApi';

interface DeviceRtuMonitorTabProps {
  device: Device | null;
  mode: 'view' | 'edit' | 'create';
}

interface CommunicationMetrics {
  timestamp: string;
  responseTime: number;
  successful: boolean;
  errorCode?: string;
  bytesTransmitted: number;
  bytesReceived: number;
}

interface RtuStatistics {
  totalRequests: number;
  successfulRequests: number;
  failedRequests: number;
  averageResponseTime: number;
  minResponseTime: number;
  maxResponseTime: number;
  lastCommunication: string;
  connectionUptime: string;
  errorRate: number;
  throughput: number; // bytes/second
}

interface NetworkDiagnostics {
  crcErrors: number;
  timeoutErrors: number;
  framingErrors: number;
  overrunErrors: number;
  parityErrors: number;
  lastErrorTime?: string;
  lastErrorMessage?: string;
}

const DeviceRtuMonitorTab: React.FC<DeviceRtuMonitorTabProps> = ({
  device,
  mode
}) => {
  const [metrics, setMetrics] = useState<CommunicationMetrics[]>([]);
  const [statistics, setStatistics] = useState<RtuStatistics | null>(null);
  const [diagnostics, setDiagnostics] = useState<NetworkDiagnostics | null>(null);
  const [isMonitoring, setIsMonitoring] = useState(false);
  const [selectedTab, setSelectedTab] = useState<'realtime' | 'statistics' | 'diagnostics'>('realtime');
  const [timeRange, setTimeRange] = useState<'1min' | '5min' | '15min' | '1hour'>('5min');
  const [error, setError] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  
  const intervalRef = useRef<NodeJS.Timeout | null>(null);
  const chartRef = useRef<HTMLCanvasElement>(null);

  // RTU 디바이스 여부 확인 - protocol_id 또는 protocol_type 기반
  const isRtuDevice = useCallback(() => {
    if (!device) return false;
    
    // 1. protocol_type으로 확인
    if (device.protocol_type === 'MODBUS_RTU') {
      return true;
    }
    
    // 2. protocol_id로 확인 (MODBUS_RTU는 보통 ID 2)
    if (device.protocol_id) {
      // DeviceApiService의 ProtocolManager를 통해 프로토콜 정보 확인
      const protocol = DeviceApiService.getProtocolManager().getProtocolById(device.protocol_id);
      return protocol?.protocol_type === 'MODBUS_RTU';
    }
    
    return false;
  }, [device]);

  // RTU 디바이스가 아니면 렌더링하지 않음
  if (!device || !isRtuDevice()) {
    return (
      <div style={{ 
        display: 'flex', 
        flexDirection: 'column', 
        alignItems: 'center', 
        justifyContent: 'center',
        padding: '2rem',
        color: '#6b7280' 
      }}>
        <i className="fas fa-info-circle" style={{ fontSize: '3rem', marginBottom: '1rem' }}></i>
        <h3 style={{ margin: '0 0 0.5rem 0', color: '#374151' }}>RTU 디바이스가 아닙니다</h3>
        <p style={{ margin: 0, textAlign: 'center' }}>
          이 탭은 Modbus RTU 프로토콜을 사용하는 디바이스에서만 표시됩니다.
        </p>
      </div>
    );
  }

  // 통계 데이터 로드
  const loadStatistics = useCallback(async () => {
    if (!device?.id) return;

    try {
      setError(null);
      console.log(`RTU 통계 로드 시작: device_id=${device.id}`);
      
      // 실제 구현에서는 RTU 통계 API 호출
      const response = await fetch(`/api/devices/rtu/${device.id}/statistics`);
      
      if (response.ok) {
        const data = await response.json();
        if (data.success) {
          setStatistics(data.data?.statistics);
          setDiagnostics(data.data?.diagnostics);
          console.log('RTU 통계 로드 완료:', data.data);
        } else {
          throw new Error(data.error || '통계 로드 실패');
        }
      } else {
        // 시뮬레이션 데이터
        console.log('API 응답 실패, 시뮬레이션 데이터 사용');
        setStatistics({
          totalRequests: 1250,
          successfulRequests: 1198,
          failedRequests: 52,
          averageResponseTime: 85,
          minResponseTime: 45,
          maxResponseTime: 230,
          lastCommunication: new Date().toISOString(),
          connectionUptime: '2d 14h 35m',
          errorRate: 4.16,
          throughput: 1024
        });

        setDiagnostics({
          crcErrors: 12,
          timeoutErrors: 28,
          framingErrors: 8,
          overrunErrors: 2,
          parityErrors: 2,
          lastErrorTime: new Date(Date.now() - 300000).toISOString(),
          lastErrorMessage: 'Response timeout on register 40001'
        });
      }
    } catch (err) {
      console.error('통계 로드 실패:', err);
      setError(err instanceof Error ? err.message : '통계를 불러올 수 없습니다');
    }
  }, [device?.id]);

  // 실시간 메트릭 수집 시작
  const startMonitoring = useCallback(() => {
    if (!device?.id || isMonitoring) return;

    setIsMonitoring(true);
    setError(null);
    setMetrics([]);

    console.log(`RTU 실시간 모니터링 시작: device_id=${device.id}`);

    // 실시간 데이터 수집 (시뮬레이션)
    intervalRef.current = setInterval(() => {
      const newMetric: CommunicationMetrics = {
        timestamp: new Date().toISOString(),
        responseTime: Math.floor(Math.random() * 150) + 50,
        successful: Math.random() > 0.05, // 95% 성공률
        errorCode: Math.random() > 0.05 ? undefined : 'TIMEOUT',
        bytesTransmitted: Math.floor(Math.random() * 64) + 8,
        bytesReceived: Math.floor(Math.random() * 128) + 16
      };

      setMetrics(prev => {
        const maxPoints = timeRange === '1min' ? 60 : 
                          timeRange === '5min' ? 300 :
                          timeRange === '15min' ? 900 : 3600;
        
        const newMetrics = [...prev, newMetric];
        return newMetrics.slice(-maxPoints);
      });
    }, 1000);

  }, [device?.id, isMonitoring, timeRange]);

  // 모니터링 중지
  const stopMonitoring = useCallback(() => {
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
    setIsMonitoring(false);
    console.log('RTU 모니터링 중지');
  }, []);

  // 통신 테스트 실행
  const runCommunicationTest = async () => {
    if (!device?.id) return;

    try {
      setIsLoading(true);
      setError(null);
      
      console.log(`RTU 통신 테스트 실행: device_id=${device.id}`);
      const response = await DeviceApiService.testDeviceConnection(device.id);
      
      if (response.success && response.data) {
        const result = response.data;
        const message = result.test_successful 
          ? `연결 성공! 응답시간: ${result.response_time_ms}ms`
          : `연결 실패: ${result.error_message}`;
        
        alert(message);
        console.log('RTU 통신 테스트 결과:', result);
        
        // 테스트 후 통계 갱신
        await loadStatistics();
      } else {
        throw new Error(response.error || '통신 테스트 실패');
      }
    } catch (err) {
      console.error('통신 테스트 실패:', err);
      setError('통신 테스트 중 오류가 발생했습니다');
      alert(`통신 테스트 실패: ${err instanceof Error ? err.message : 'Unknown error'}`);
    } finally {
      setIsLoading(false);
    }
  };

  // 차트 그리기 (간단한 Canvas 구현)
  const drawChart = useCallback(() => {
    const canvas = chartRef.current;
    if (!canvas || metrics.length === 0) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const width = canvas.width;
    const height = canvas.height;
    const padding = 40;

    // 캔버스 클리어
    ctx.clearRect(0, 0, width, height);

    // 배경
    ctx.fillStyle = '#f9fafb';
    ctx.fillRect(0, 0, width, height);

    // 데이터 범위 계산
    const responseTimes = metrics.map(m => m.responseTime);
    const minTime = Math.min(...responseTimes);
    const maxTime = Math.max(...responseTimes);
    const timeRange = maxTime - minTime || 100;

    // 그리드 그리기
    ctx.strokeStyle = '#e5e7eb';
    ctx.lineWidth = 1;
    
    // 세로 그리드 (시간)
    for (let i = 0; i <= 5; i++) {
      const x = padding + (i * (width - 2 * padding)) / 5;
      ctx.beginPath();
      ctx.moveTo(x, padding);
      ctx.lineTo(x, height - padding);
      ctx.stroke();
    }

    // 가로 그리드 (응답시간)
    for (let i = 0; i <= 4; i++) {
      const y = padding + (i * (height - 2 * padding)) / 4;
      ctx.beginPath();
      ctx.moveTo(padding, y);
      ctx.lineTo(width - padding, y);
      ctx.stroke();
    }

    // 응답시간 라인 그리기
    ctx.strokeStyle = '#0ea5e9';
    ctx.lineWidth = 2;
    ctx.beginPath();

    metrics.forEach((metric, index) => {
      const x = padding + (index * (width - 2 * padding)) / (metrics.length - 1);
      const y = height - padding - ((metric.responseTime - minTime) * (height - 2 * padding)) / timeRange;
      
      if (index === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }

      // 실패한 요청은 빨간 점으로 표시
      if (!metric.successful) {
        ctx.fillStyle = '#dc2626';
        ctx.beginPath();
        ctx.arc(x, y, 3, 0, 2 * Math.PI);
        ctx.fill();
      }
    });

    ctx.stroke();

    // Y축 레이블
    ctx.fillStyle = '#6b7280';
    ctx.font = '12px sans-serif';
    ctx.textAlign = 'right';
    
    for (let i = 0; i <= 4; i++) {
      const value = minTime + (i * timeRange) / 4;
      const y = height - padding - (i * (height - 2 * padding)) / 4;
      ctx.fillText(`${Math.round(value)}ms`, padding - 10, y + 4);
    }

  }, [metrics]);

  // 차트 업데이트
  useEffect(() => {
    drawChart();
  }, [metrics, drawChart]);

  // 컴포넌트 마운트 시 통계 로드
  useEffect(() => {
    loadStatistics();
  }, [loadStatistics]);

  // 컴포넌트 언마운트 시 모니터링 정지
  useEffect(() => {
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, []);

  // 프로토콜 정보 가져오기
  const getProtocolInfo = () => {
    if (device?.protocol_id) {
      return DeviceApiService.getProtocolManager().getProtocolById(device.protocol_id);
    }
    return DeviceApiService.getProtocolManager().getProtocolByType(device?.protocol_type || '');
  };

  const protocolInfo = getProtocolInfo();

  // 실시간 탭
  const renderRealtimeTab = () => (
    <div className="realtime-monitor">
      {/* 컨트롤 패널 */}
      <div className="control-panel">
        <div className="monitor-controls">
          <button
            className={`btn ${isMonitoring ? 'btn-danger' : 'btn-primary'}`}
            onClick={isMonitoring ? stopMonitoring : startMonitoring}
            disabled={isLoading}
          >
            <i className={`fas ${isMonitoring ? 'fa-stop' : 'fa-play'}`}></i>
            {isMonitoring ? '모니터링 중지' : '모니터링 시작'}
          </button>
          
          <select
            value={timeRange}
            onChange={(e) => setTimeRange(e.target.value as typeof timeRange)}
            disabled={isMonitoring}
          >
            <option value="1min">1분</option>
            <option value="5min">5분</option>
            <option value="15min">15분</option>
            <option value="1hour">1시간</option>
          </select>
          
          <button
            className="btn btn-info"
            onClick={runCommunicationTest}
            disabled={isLoading}
          >
            <i className="fas fa-vial"></i>
            통신 테스트
          </button>
        </div>
        
        <div className="live-stats">
          {isMonitoring && metrics.length > 0 && (
            <>
              <div className="stat-item">
                <label>현재 응답시간</label>
                <span className="stat-value">{metrics[metrics.length - 1]?.responseTime || 0}ms</span>
              </div>
              <div className="stat-item">
                <label>평균 응답시간</label>
                <span className="stat-value">
                  {Math.round(metrics.reduce((sum, m) => sum + m.responseTime, 0) / metrics.length)}ms
                </span>
              </div>
              <div className="stat-item">
                <label>성공률</label>
                <span className="stat-value success">
                  {Math.round((metrics.filter(m => m.successful).length / metrics.length) * 100)}%
                </span>
              </div>
            </>
          )}
        </div>
      </div>

      {/* 실시간 차트 */}
      <div className="chart-container">
        <div className="chart-header">
          <h4>응답시간 추이</h4>
          <div className="chart-legend">
            <div className="legend-item">
              <div className="legend-color line"></div>
              <span>응답시간</span>
            </div>
            <div className="legend-item">
              <div className="legend-color error"></div>
              <span>통신 실패</span>
            </div>
          </div>
        </div>
        
        <canvas
          ref={chartRef}
          width={600}
          height={300}
          className="response-time-chart"
        />
        
        {metrics.length === 0 && !isMonitoring && (
          <div className="chart-empty">
            <i className="fas fa-chart-line"></i>
            <p>모니터링을 시작하면 실시간 데이터가 표시됩니다</p>
          </div>
        )}
      </div>

      {/* 최근 통신 로그 */}
      {metrics.length > 0 && (
        <div className="communication-log">
          <h4>최근 통신 기록</h4>
          <div className="log-table">
            <div className="log-header">
              <div>시간</div>
              <div>상태</div>
              <div>응답시간</div>
              <div>송신</div>
              <div>수신</div>
            </div>
            <div className="log-body">
              {metrics.slice(-10).reverse().map((metric, index) => (
                <div key={index} className="log-row">
                  <div className="log-time">
                    {new Date(metric.timestamp).toLocaleTimeString()}
                  </div>
                  <div className={`log-status ${metric.successful ? 'success' : 'error'}`}>
                    <i className={`fas ${metric.successful ? 'fa-check' : 'fa-times'}`}></i>
                    {metric.successful ? '성공' : metric.errorCode || '실패'}
                  </div>
                  <div className="log-response-time">
                    {metric.responseTime}ms
                  </div>
                  <div className="log-bytes">
                    {metric.bytesTransmitted}B
                  </div>
                  <div className="log-bytes">
                    {metric.bytesReceived}B
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}
    </div>
  );

  // 통계 탭
  const renderStatisticsTab = () => (
    <div className="statistics-view">
      {statistics && (
        <>
          {/* 통계 카드들 */}
          <div className="stats-grid">
            <div className="stat-card">
              <div className="stat-icon">
                <i className="fas fa-exchange-alt"></i>
              </div>
              <div className="stat-content">
                <div className="stat-value">{statistics.totalRequests.toLocaleString()}</div>
                <div className="stat-label">총 요청</div>
              </div>
            </div>
            
            <div className="stat-card">
              <div className="stat-icon success">
                <i className="fas fa-check-circle"></i>
              </div>
              <div className="stat-content">
                <div className="stat-value success">{statistics.successfulRequests.toLocaleString()}</div>
                <div className="stat-label">성공한 요청</div>
              </div>
            </div>
            
            <div className="stat-card">
              <div className="stat-icon error">
                <i className="fas fa-times-circle"></i>
              </div>
              <div className="stat-content">
                <div className="stat-value error">{statistics.failedRequests.toLocaleString()}</div>
                <div className="stat-label">실패한 요청</div>
              </div>
            </div>
            
            <div className="stat-card">
              <div className="stat-icon">
                <i className="fas fa-clock"></i>
              </div>
              <div className="stat-content">
                <div className="stat-value">{statistics.averageResponseTime}ms</div>
                <div className="stat-label">평균 응답시간</div>
              </div>
            </div>
            
            <div className="stat-card">
              <div className="stat-icon">
                <i className="fas fa-tachometer-alt"></i>
              </div>
              <div className="stat-content">
                <div className="stat-value">{(statistics.throughput / 1024).toFixed(1)}KB/s</div>
                <div className="stat-label">처리량</div>
              </div>
            </div>
            
            <div className="stat-card">
              <div className="stat-icon">
                <i className="fas fa-exclamation-triangle"></i>
              </div>
              <div className="stat-content">
                <div className="stat-value error">{statistics.errorRate.toFixed(1)}%</div>
                <div className="stat-label">오류율</div>
              </div>
            </div>
          </div>

          {/* 상세 통계 */}
          <div className="detailed-stats">
            <div className="stats-section">
              <h4>응답시간 통계</h4>
              <div className="stats-table">
                <div className="stats-row">
                  <span>평균:</span>
                  <span>{statistics.averageResponseTime}ms</span>
                </div>
                <div className="stats-row">
                  <span>최소:</span>
                  <span>{statistics.minResponseTime}ms</span>
                </div>
                <div className="stats-row">
                  <span>최대:</span>
                  <span>{statistics.maxResponseTime}ms</span>
                </div>
              </div>
            </div>

            <div className="stats-section">
              <h4>연결 정보</h4>
              <div className="stats-table">
                <div className="stats-row">
                  <span>연결 유지 시간:</span>
                  <span>{statistics.connectionUptime}</span>
                </div>
                <div className="stats-row">
                  <span>마지막 통신:</span>
                  <span>{new Date(statistics.lastCommunication).toLocaleString()}</span>
                </div>
                <div className="stats-row">
                  <span>성공률:</span>
                  <span className="success">
                    {((statistics.successfulRequests / statistics.totalRequests) * 100).toFixed(2)}%
                  </span>
                </div>
              </div>
            </div>

            {/* 프로토콜 정보 */}
            {protocolInfo && (
              <div className="stats-section">
                <h4>프로토콜 정보</h4>
                <div className="stats-table">
                  <div className="stats-row">
                    <span>프로토콜:</span>
                    <span>{protocolInfo.display_name || protocolInfo.name}</span>
                  </div>
                  <div className="stats-row">
                    <span>프로토콜 ID:</span>
                    <span>{protocolInfo.id}</span>
                  </div>
                  {protocolInfo.default_port && (
                    <div className="stats-row">
                      <span>기본 포트:</span>
                      <span>{protocolInfo.default_port}</span>
                    </div>
                  )}
                  {protocolInfo.category && (
                    <div className="stats-row">
                      <span>카테고리:</span>
                      <span>{protocolInfo.category}</span>
                    </div>
                  )}
                </div>
              </div>
            )}
          </div>
        </>
      )}
      
      <div className="stats-actions">
        <button className="btn btn-secondary" onClick={loadStatistics}>
          <i className="fas fa-sync-alt"></i>
          통계 새로고침
        </button>
      </div>
    </div>
  );

  // 진단 탭
  const renderDiagnosticsTab = () => (
    <div className="diagnostics-view">
      {diagnostics && (
        <>
          {/* 오류 통계 */}
          <div className="error-stats">
            <h4>통신 오류 분석</h4>
            <div className="error-grid">
              <div className="error-card">
                <div className="error-icon">
                  <i className="fas fa-clock"></i>
                </div>
                <div className="error-content">
                  <div className="error-count">{diagnostics.timeoutErrors}</div>
                  <div className="error-label">타임아웃 오류</div>
                </div>
              </div>
              
              <div className="error-card">
                <div className="error-icon">
                  <i className="fas fa-shield-alt"></i>
                </div>
                <div className="error-content">
                  <div className="error-count">{diagnostics.crcErrors}</div>
                  <div className="error-label">CRC 오류</div>
                </div>
              </div>
              
              <div className="error-card">
                <div className="error-icon">
                  <i className="fas fa-align-left"></i>
                </div>
                <div className="error-content">
                  <div className="error-count">{diagnostics.framingErrors}</div>
                  <div className="error-label">프레이밍 오류</div>
                </div>
              </div>
              
              <div className="error-card">
                <div className="error-icon">
                  <i className="fas fa-database"></i>
                </div>
                <div className="error-content">
                  <div className="error-count">{diagnostics.overrunErrors}</div>
                  <div className="error-label">오버런 오류</div>
                </div>
              </div>
              
              <div className="error-card">
                <div className="error-icon">
                  <i className="fas fa-balance-scale"></i>
                </div>
                <div className="error-content">
                  <div className="error-count">{diagnostics.parityErrors}</div>
                  <div className="error-label">패리티 오류</div>
                </div>
              </div>
            </div>
          </div>

          {/* 최근 오류 정보 */}
          {diagnostics.lastErrorTime && (
            <div className="last-error">
              <h4>최근 오류</h4>
              <div className="error-info">
                <div className="error-time">
                  <i className="fas fa-clock"></i>
                  {new Date(diagnostics.lastErrorTime).toLocaleString()}
                </div>
                <div className="error-message">
                  <i className="fas fa-exclamation-triangle"></i>
                  {diagnostics.lastErrorMessage}
                </div>
              </div>
            </div>
          )}

          {/* 네트워크 상태 */}
          <div className="network-health">
            <h4>네트워크 상태</h4>
            <div className="health-indicators">
              <div className="health-indicator">
                <div className="indicator-icon success">
                  <i className="fas fa-wifi"></i>
                </div>
                <div className="indicator-content">
                  <div className="indicator-label">신호 품질</div>
                  <div className="indicator-value">양호</div>
                </div>
              </div>
              
              <div className="health-indicator">
                <div className="indicator-icon warning">
                  <i className="fas fa-exclamation-triangle"></i>
                </div>
                <div className="indicator-content">
                  <div className="indicator-label">오류율</div>
                  <div className="indicator-value">
                    {((diagnostics.crcErrors + diagnostics.timeoutErrors + 
                       diagnostics.framingErrors + diagnostics.overrunErrors + 
                       diagnostics.parityErrors) / (statistics?.totalRequests || 1) * 100).toFixed(2)}%
                  </div>
                </div>
              </div>
            </div>
          </div>

          {/* 권장사항 */}
          <div className="recommendations">
            <h4>개선 권장사항</h4>
            <div className="recommendation-list">
              {diagnostics.timeoutErrors > 20 && (
                <div className="recommendation-item">
                  <i className="fas fa-lightbulb"></i>
                  <span>타임아웃 오류가 많습니다. Response Timeout 값을 늘려보세요.</span>
                </div>
              )}
              
              {diagnostics.crcErrors > 10 && (
                <div className="recommendation-item">
                  <i className="fas fa-lightbulb"></i>
                  <span>CRC 오류가 발생하고 있습니다. 배선 상태를 확인하세요.</span>
                </div>
              )}
              
              {diagnostics.framingErrors > 5 && (
                <div className="recommendation-item">
                  <i className="fas fa-lightbulb"></i>
                  <span>프레이밍 오류가 있습니다. Baud Rate 설정을 확인하세요.</span>
                </div>
              )}
              
              {(diagnostics.crcErrors + diagnostics.timeoutErrors + diagnostics.framingErrors + 
                diagnostics.overrunErrors + diagnostics.parityErrors) === 0 && (
                <div className="recommendation-item success">
                  <i className="fas fa-check-circle"></i>
                  <span>통신 상태가 양호합니다.</span>
                </div>
              )}
            </div>
          </div>
        </>
      )}
    </div>
  );

  return (
    <div className="rtu-monitor-tab">
      {/* 탭 헤더 */}
      <div className="monitor-tab-header">
        <div className="tab-navigation">
          <button
            className={`tab-btn ${selectedTab === 'realtime' ? 'active' : ''}`}
            onClick={() => setSelectedTab('realtime')}
          >
            <i className="fas fa-chart-line"></i>
            실시간 모니터링
          </button>
          <button
            className={`tab-btn ${selectedTab === 'statistics' ? 'active' : ''}`}
            onClick={() => setSelectedTab('statistics')}
          >
            <i className="fas fa-chart-bar"></i>
            통계
          </button>
          <button
            className={`tab-btn ${selectedTab === 'diagnostics' ? 'active' : ''}`}
            onClick={() => setSelectedTab('diagnostics')}
          >
            <i className="fas fa-stethoscope"></i>
            진단
          </button>
        </div>
        
        <div className="device-status">
          <span className={`status-indicator ${device.connection_status || 'unknown'}`}>
            <i className="fas fa-circle"></i>
            {device.connection_status === 'connected' ? '연결됨' :
             device.connection_status === 'disconnected' ? '연결끊김' : '알수없음'}
          </span>
          {protocolInfo && (
            <span className="protocol-info">
              {protocolInfo.display_name || protocolInfo.name} (ID: {protocolInfo.id})
            </span>
          )}
        </div>
      </div>

      {/* 오류 메시지 */}
      {error && (
        <div className="error-banner">
          <i className="fas fa-exclamation-triangle"></i>
          <span>{error}</span>
          <button onClick={() => setError(null)}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 탭 내용 */}
      <div className="monitor-content">
        {selectedTab === 'realtime' && renderRealtimeTab()}
        {selectedTab === 'statistics' && renderStatisticsTab()}
        {selectedTab === 'diagnostics' && renderDiagnosticsTab()}
      </div>

      <style jsx>{`
        .rtu-monitor-tab {
          flex: 1;
          display: flex;
          flex-direction: column;
          background: #f8fafc;
        }

        .monitor-tab-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 1rem 1.5rem;
          background: white;
          border-bottom: 1px solid #e5e7eb;
          flex-shrink: 0;
        }

        .tab-navigation {
          display: flex;
          gap: 0.5rem;
        }

        .tab-btn {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border: none;
          background: #f3f4f6;
          color: #6b7280;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s ease;
        }

        .tab-btn:hover {
          background: #e5e7eb;
          color: #374151;
        }

        .tab-btn.active {
          background: #0ea5e9;
          color: white;
        }

        .device-status {
          display: flex;
          align-items: center;
          gap: 1rem;
        }

        .status-indicator {
          display: flex;
          align-items: center;
          gap: 0.375rem;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .status-indicator.connected {
          background: #dcfce7;
          color: #166534;
        }

        .status-indicator.disconnected {
          background: #fee2e2;
          color: #991b1b;
        }

        .status-indicator.unknown {
          background: #f3f4f6;
          color: #6b7280;
        }

        .protocol-info {
          font-size: 0.75rem;
          color: #6b7280;
          background: #f3f4f6;
          padding: 0.25rem 0.5rem;
          border-radius: 0.25rem;
        }

        .error-banner {
          display: flex;
          align-items: center;
          gap: 0.75rem;
          padding: 1rem 1.5rem;
          background: #fef2f2;
          color: #dc2626;
          border-bottom: 1px solid #fecaca;
        }

        .error-banner button {
          margin-left: auto;
          border: none;
          background: none;
          color: #dc2626;
          cursor: pointer;
        }

        .monitor-content {
          flex: 1;
          overflow-y: auto;
          padding: 1.5rem;
        }

        .control-panel {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 2rem;
          padding: 1rem;
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
        }

        .monitor-controls {
          display: flex;
          gap: 0.75rem;
          align-items: center;
        }

        .monitor-controls select {
          padding: 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
        }

        .live-stats {
          display: flex;
          gap: 1.5rem;
        }

        .stat-item {
          display: flex;
          flex-direction: column;
          text-align: center;
          min-width: 80px;
        }

        .stat-item label {
          font-size: 0.75rem;
          color: #6b7280;
          margin-bottom: 0.25rem;
        }

        .stat-value {
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .stat-value.success {
          color: #059669;
        }

        .chart-container {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          padding: 1.5rem;
          margin-bottom: 2rem;
        }

        .chart-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 1rem;
        }

        .chart-header h4 {
          margin: 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .chart-legend {
          display: flex;
          gap: 1rem;
        }

        .legend-item {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          font-size: 0.75rem;
          color: #6b7280;
        }

        .legend-color {
          width: 12px;
          height: 12px;
          border-radius: 2px;
        }

        .legend-color.line {
          background: #0ea5e9;
        }

        .legend-color.error {
          background: #dc2626;
          border-radius: 50%;
        }

        .response-time-chart {
          width: 100%;
          height: auto;
          border: 1px solid #e5e7eb;
          border-radius: 0.375rem;
        }

        .chart-empty {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          height: 300px;
          color: #6b7280;
        }

        .chart-empty i {
          font-size: 3rem;
          margin-bottom: 1rem;
        }

        .communication-log {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          padding: 1.5rem;
        }

        .communication-log h4 {
          margin: 0 0 1rem 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .log-table {
          border: 1px solid #e5e7eb;
          border-radius: 0.375rem;
          overflow: hidden;
        }

        .log-header {
          display: grid;
          grid-template-columns: 1fr 1fr 1fr 80px 80px;
          gap: 1rem;
          padding: 0.75rem 1rem;
          background: #f9fafb;
          font-size: 0.75rem;
          font-weight: 600;
          color: #374151;
          border-bottom: 1px solid #e5e7eb;
        }

        .log-body {
          max-height: 200px;
          overflow-y: auto;
        }

        .log-row {
          display: grid;
          grid-template-columns: 1fr 1fr 1fr 80px 80px;
          gap: 1rem;
          padding: 0.75rem 1rem;
          border-bottom: 1px solid #f3f4f6;
          font-size: 0.75rem;
          color: #374151;
        }

        .log-row:last-child {
          border-bottom: none;
        }

        .log-status {
          display: flex;
          align-items: center;
          gap: 0.375rem;
        }

        .log-status.success {
          color: #059669;
        }

        .log-status.error {
          color: #dc2626;
        }

        .stats-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 1rem;
          margin-bottom: 2rem;
        }

        .stat-card {
          display: flex;
          align-items: center;
          gap: 1rem;
          padding: 1.5rem;
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
        }

        .stat-icon {
          width: 3rem;
          height: 3rem;
          display: flex;
          align-items: center;
          justify-content: center;
          background: #f3f4f6;
          border-radius: 0.5rem;
          color: #6b7280;
          font-size: 1.25rem;
        }

        .stat-icon.success {
          background: #dcfce7;
          color: #166534;
        }

        .stat-icon.error {
          background: #fee2e2;
          color: #991b1b;
        }

        .stat-content {
          flex: 1;
        }

        .stat-card .stat-value {
          font-size: 1.5rem;
          font-weight: 600;
          color: #1f2937;
          line-height: 1;
          margin-bottom: 0.25rem;
        }

        .stat-card .stat-value.success {
          color: #059669;
        }

        .stat-card .stat-value.error {
          color: #dc2626;
        }

        .stat-card .stat-label {
          font-size: 0.875rem;
          color: #6b7280;
        }

        .detailed-stats {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
          gap: 1.5rem;
          margin-bottom: 2rem;
        }

        .stats-section {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          padding: 1.5rem;
        }

        .stats-section h4 {
          margin: 0 0 1rem 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .stats-table {
          display: flex;
          flex-direction: column;
          gap: 0.5rem;
        }

        .stats-row {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 0.5rem 0;
          border-bottom: 1px solid #f3f4f6;
          font-size: 0.875rem;
        }

        .stats-row:last-child {
          border-bottom: none;
        }

        .stats-row span:first-child {
          color: #6b7280;
        }

        .stats-row span:last-child {
          font-weight: 600;
          color: #374151;
        }

        .stats-row .success {
          color: #059669;
        }

        .stats-actions {
          display: flex;
          justify-content: center;
        }

        .error-stats {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          padding: 1.5rem;
          margin-bottom: 2rem;
        }

        .error-stats h4 {
          margin: 0 0 1rem 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .error-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
          gap: 1rem;
        }

        .error-card {
          display: flex;
          flex-direction: column;
          align-items: center;
          padding: 1rem;
          background: #fef2f2;
          border: 1px solid #fecaca;
          border-radius: 0.5rem;
        }

        .error-icon {
          width: 2.5rem;
          height: 2.5rem;
          display: flex;
          align-items: center;
          justify-content: center;
          background: #fee2e2;
          border-radius: 0.375rem;
          color: #dc2626;
          font-size: 1.125rem;
          margin-bottom: 0.75rem;
        }

        .error-count {
          font-size: 1.5rem;
          font-weight: 700;
          color: #dc2626;
          line-height: 1;
          margin-bottom: 0.25rem;
        }

        .error-label {
          font-size: 0.75rem;
          color: #991b1b;
          text-align: center;
        }

        .last-error,
        .network-health,
        .recommendations {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          padding: 1.5rem;
          margin-bottom: 2rem;
        }

        .last-error h4,
        .network-health h4,
        .recommendations h4 {
          margin: 0 0 1rem 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .error-info {
          display: flex;
          flex-direction: column;
          gap: 0.5rem;
        }

        .error-time,
        .error-message {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          font-size: 0.875rem;
        }

        .error-time {
          color: #6b7280;
        }

        .error-message {
          color: #dc2626;
        }

        .health-indicators {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 1rem;
        }

        .health-indicator {
          display: flex;
          align-items: center;
          gap: 1rem;
          padding: 1rem;
          background: #f9fafb;
          border-radius: 0.5rem;
        }

        .indicator-icon {
          width: 2.5rem;
          height: 2.5rem;
          display: flex;
          align-items: center;
          justify-content: center;
          border-radius: 0.375rem;
          font-size: 1.125rem;
        }

        .indicator-icon.success {
          background: #dcfce7;
          color: #166534;
        }

        .indicator-icon.warning {
          background: #fef3c7;
          color: #d97706;
        }

        .indicator-label {
          font-size: 0.75rem;
          color: #6b7280;
          margin-bottom: 0.25rem;
        }

        .indicator-value {
          font-size: 0.875rem;
          font-weight: 600;
          color: #374151;
        }

        .recommendation-list {
          display: flex;
          flex-direction: column;
          gap: 0.75rem;
        }

        .recommendation-item {
          display: flex;
          align-items: flex-start;
          gap: 0.75rem;
          padding: 0.75rem;
          background: #fffbeb;
          border: 1px solid #fed7aa;
          border-radius: 0.5rem;
          font-size: 0.875rem;
          color: #92400e;
        }

        .recommendation-item.success {
          background: #f0fdf4;
          border-color: #bbf7d0;
          color: #166534;
        }

        .recommendation-item i {
          margin-top: 0.125rem;
          flex-shrink: 0;
        }

        .btn {
          display: inline-flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border: none;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s ease;
        }

        .btn-primary {
          background: #0ea5e9;
          color: white;
        }

        .btn-secondary {
          background: #64748b;
          color: white;
        }

        .btn-danger {
          background: #dc2626;
          color: white;
        }

        .btn-info {
          background: #0891b2;
          color: white;
        }

        .btn:hover:not(:disabled) {
          opacity: 0.9;
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }
      `}</style>
    </div>
  );
};

export default DeviceRtuMonitorTab;