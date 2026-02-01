// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceStatusTab.tsx
// 📈 디바이스 상태 탭 컴포넌트 - Pro-Engineer Density (Horizontal Layout)
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceApiService } from '../../../api/services/deviceApi';
import { DataApiService } from '../../../api/services/dataApi';
import { DeviceStatusTabProps } from './types';
import '../../../styles/management.css';

const DeviceStatusTab: React.FC<DeviceStatusTabProps> = ({ device, dataPoints }) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [isTestingConnection, setIsTestingConnection] = useState(false);
  const [isDiagnosing, setIsDiagnosing] = useState(false);
  const [connectionTestResult, setConnectionTestResult] = useState<any>(null);
  const [diagnoseResult, setDiagnoseResult] = useState<any>(null);
  const [deviceDataPoints, setDeviceDataPoints] = useState<any[]>(dataPoints || []);
  const [isLoadingStats, setIsLoadingStats] = useState(false);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());

  const displayData = device;
  const statusInfo = displayData?.status_info || displayData?.status || {};

  // ========================================================================
  // API 호출 함수들
  // ========================================================================

  /**
   * 연결 테스트 실행
   */
  const handleTestConnection = async () => {
    if (!device?.id) return;

    try {
      setIsTestingConnection(true);
      const response = await DeviceApiService.testDeviceConnection(device.id);

      if (response.success && response.data) {
        setConnectionTestResult(response.data);
      } else {
        setConnectionTestResult({
          test_successful: false,
          error_message: response.error || '테스트 실패'
        });
      }
    } catch (error) {
      console.error('연결 테스트 실패:', error);
      setConnectionTestResult({
        test_successful: false,
        error_message: error instanceof Error ? error.message : 'Unknown error'
      });
    } finally {
      setIsTestingConnection(false);
    }
  };

  /**
   * 실시간 연결 진단 실행
   */
  const handleDiagnose = async () => {
    if (!device?.id) return;

    try {
      setIsDiagnosing(true);
      setDiagnoseResult(null);
      const response = await DeviceApiService.diagnoseDevice(device.id);

      if (response.success && response.data) {
        setDiagnoseResult(response.data);
      } else {
        setDiagnoseResult({
          success: false,
          message: response.error || '진단 실패'
        });
      }
    } catch (error) {
      console.error('연결 진단 실패:', error);
      setDiagnoseResult({
        success: false,
        message: error instanceof Error ? error.message : 'Unknown error'
      });
    } finally {
      setIsDiagnosing(false);
    }
  };

  /**
   * 디바이스 통계 및 데이터포인트 로드
   */
  const loadDeviceStats = async () => {
    if (!device?.id) return;

    // 만약 dataPoints prop이 있으면 API 호출 건너뜀 (또는 갱신 목적으로 할 수도 있지만 일단 prop 우선)
    if (dataPoints && dataPoints.length > 0) {
      setDeviceDataPoints(dataPoints);
      return;
    }

    try {
      setIsLoadingStats(true);

      // 데이터포인트 목록 조회 (최신값 포함)
      const response = await DataApiService.searchDataPoints({
        device_id: device.id,
        include_current_value: true,
        limit: 100 // 적절한 수량 제한
      } as any); // Type assertion to bypass lint error for now

      if (response.success && response.data && response.data.items) {
        setDeviceDataPoints(response.data.items);
      }
    } catch (error) {
      console.error('디바이스 데이터포인트 로드 실패:', error);
    } finally {
      setIsLoadingStats(false);
    }
  };

  /**
   * 상태 새로고침
   */
  const handleRefreshStatus = () => {
    setLastUpdate(new Date());
    // dataPoints prop이 있어도 새로고침 시에는 다시 불러오거나, 부모에게 요청해야 함.
    // 여기서는 독립적 fetching 시도
    loadDeviceStatsFromApi();
  };

  const loadDeviceStatsFromApi = async () => {
    if (!device?.id) return;
    try {
      setIsLoadingStats(true);
      const response = await DataApiService.searchDataPoints({
        device_id: device.id,
        include_current_value: true,
        limit: 100
      } as any);

      if (response.success && response.data && response.data.items) {
        setDeviceDataPoints(response.data.items);
      }
    } catch (error) {
      console.error(error);
    } finally {
      setIsLoadingStats(false);
    }
  }

  // ========================================================================
  // 라이프사이클
  // ========================================================================

  useEffect(() => {
    if (dataPoints) {
      setDeviceDataPoints(dataPoints);
    } else if (device?.id) {
      loadDeviceStats();
    }
  }, [device?.id, dataPoints]);

  // ========================================================================
  // 헬퍼 함수들
  // ========================================================================

  /**
   * 상태 색상 함수
   */
  const getStatusColor = (status?: string) => {
    switch (status) {
      case 'connected': return 'text-success-600';
      case 'disconnected': return 'text-error-600';
      case 'connecting': return 'text-warning-600';
      case 'error': return 'text-error-600';
      default: return 'text-neutral-500';
    }
  };

  /**
   * 성공률 계산
   */
  const calculateSuccessRate = () => {
    const total = (statusInfo.total_requests || 0);
    const successful = (statusInfo.successful_requests || 0);
    return total > 0 ? ((successful / total) * 100).toFixed(1) : 'N/A';
  };

  /**
   * 시간 포맷 함수
   */
  const formatTimeAgo = (dateString?: string) => {
    if (!dateString) return 'N/A';
    const diff = Math.floor((Date.now() - new Date(dateString).getTime()) / 60000);
    if (diff < 1) return '방금 전';
    if (diff < 60) return `${diff}분 전`;
    if (diff < 1440) return `${Math.floor(diff / 60)}시간 전`;
    return `${Math.floor(diff / 1440)}일 전`;
  };

  /**
   * 렌더링 헬퍼: 읽기 전용 필드 (라벨 - 값 1줄 배치)
   * Compact version for dense grid
   */
  const renderReadOnlyField = (label: string, value: React.ReactNode, valueClass: string = '') => (
    <div className="status-field">
      <span className="field-label">{label}</span>
      <span className={`field-value ${valueClass}`}>{value}</span>
    </div>
  );

  // ========================================================================
  // 렌더링
  // ========================================================================

  return (
    <div className="status-tab-wrapper">
      {/* 헤더 & 컨트롤 */}
      <div className="status-header">
        <div className="status-time">
          마지막 업데이트: {lastUpdate.toLocaleTimeString()}
        </div>
        <div className="status-actions">
          <button
            className="st-btn st-btn-secondary"
            onClick={handleRefreshStatus}
            disabled={isLoadingStats}
          >
            <i className={`fas fa-sync ${isLoadingStats ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
          <button
            className="st-btn st-btn-primary"
            onClick={handleTestConnection}
            disabled={isTestingConnection || isDiagnosing}
          >
            {isTestingConnection ? (
              <><i className="fas fa-spinner fa-spin"></i> 테스트 중...</>
            ) : (
              <><i className="fas fa-plug"></i> 연결 테스트</>
            )}
          </button>
          <button
            className="st-btn st-btn-accent"
            onClick={handleDiagnose}
            disabled={isTestingConnection || isDiagnosing}
          >
            {isDiagnosing ? (
              <><i className="fas fa-spinner fa-spin"></i> 진단 중...</>
            ) : (
              <><i className="fas fa-stethoscope"></i> 정밀 진단</>
            )}
          </button>
        </div>
      </div>

      {/* 가로형 그리드 레이아웃 (스크롤 방지) */}
      <div className="status-grid">

        {/* 1. 연결 상태 (Connection) */}
        <div className="status-card">
          <h3><i className="fas fa-link icon-con"></i> 연결 상태</h3>
          <div className="status-content">
            {renderReadOnlyField('현재 상태', (
              <span className={`status-badge-row ${getStatusColor(displayData?.connection_status)}`}>
                <i className={`fas ${displayData?.connection_status === 'connected' ? 'fa-check-circle' : 'fa-times-circle'}`}></i>
                {displayData?.connection_status === 'connected' ? '연결됨' : displayData?.connection_status === 'disconnected' ? '연결끊김' : '확인 중'}
              </span>
            ))}
            {renderReadOnlyField('마지막 통신', formatTimeAgo(statusInfo.last_communication || displayData?.last_seen))}
            {renderReadOnlyField('응답 시간', statusInfo.response_time ? `${statusInfo.response_time}ms` : 'N/A')}
            {renderReadOnlyField('오류 횟수', `${statusInfo.error_count || 0}회`, (statusInfo.error_count || 0) > 0 ? 'text-error-600' : '')}

            {statusInfo.last_error && (
              <div className="error-box">
                <span className="error-label">Last Error:</span>
                <span className="error-text">{statusInfo.last_error}</span>
              </div>
            )}

            {diagnoseResult && (
              <div className={`diagnose-result ${diagnoseResult.success ? 'success' : 'error'}`}>
                <div className="diag-header">
                  <i className={`fas ${diagnoseResult.success ? 'fa-check-circle' : 'fa-exclamation-circle'}`}></i>
                  <strong>진단 결과: {diagnoseResult.success ? '정상' : '오류'}</strong>
                </div>
                <div className="diag-body">
                  {diagnoseResult.message && <p>{diagnoseResult.message}</p>}
                  {diagnoseResult.details && (
                    <ul className="diag-details">
                      {Object.entries(diagnoseResult.details).map(([key, value]: [string, any]) => (
                        <li key={key} style={{ wordBreak: 'break-all' }}>
                          <strong>{key}:</strong>
                          {typeof value === 'object' ? (
                            <pre style={{
                              whiteSpace: 'pre-wrap',
                              wordBreak: 'break-all',
                              fontSize: '10px',
                              background: 'rgba(0,0,0,0.03)',
                              padding: '4px',
                              borderRadius: '4px',
                              marginTop: '4px'
                            }}>
                              {JSON.stringify(value, null, 2)}
                            </pre>
                          ) : String(value)}
                        </li>
                      ))}
                    </ul>
                  )}
                </div>
                <button className="diag-close" onClick={() => setDiagnoseResult(null)}>×</button>
              </div>
            )}
          </div>
        </div>

        {/* 2. 성능 통계 (Performance) */}
        <div className="status-card">
          <h3><i className="fas fa-tachometer-alt icon-perf"></i> 성능 통계</h3>
          <div className="status-content">
            {renderReadOnlyField('총 요청', (statusInfo.total_requests || 0).toLocaleString())}
            {renderReadOnlyField('성공률', `${calculateSuccessRate()}%`)}
            {renderReadOnlyField('성공', (statusInfo.successful_requests || 0).toLocaleString(), 'text-success-600')}
            {renderReadOnlyField('실패', (statusInfo.failed_requests || 0).toLocaleString(), 'text-error-600')}
            {renderReadOnlyField('처리량', `${(statusInfo.throughput_ops_per_sec || 0).toFixed(2)} ops/sec`)}
          </div>
        </div>

        {/* 3. 시스템 정보 (System) */}
        <div className="status-card">
          <h3><i className="fas fa-microchip icon-sys"></i> 시스템 정보</h3>
          <div className="status-content">
            {renderReadOnlyField('펌웨어', statusInfo.firmware_version || 'N/A')}
            {renderReadOnlyField('하드웨어', statusInfo.hardware_info || 'N/A')}
            {renderReadOnlyField('CPU', `${statusInfo.cpu_usage || 0}%`)}
            {renderReadOnlyField('메모리', `${statusInfo.memory_usage || 0}%`)}
            {renderReadOnlyField('Uptime', `${statusInfo.uptime_percentage || 0}%`)}
          </div>
        </div>

        {/* 4. 데이터 포인트 (Data Points List) */}
        <div className="status-card">
          <h3><i className="fas fa-database icon-data"></i> 데이터 포인트 ({deviceDataPoints.length})</h3>
          <div className="status-content-scrollable">
            {deviceDataPoints.length > 0 ? (
              <div className="metrics-list">
                {deviceDataPoints.map((point: any) => (
                  <div key={point.id} className="metric-item-row">
                    <span className="metric-name" title={point.name}>{point.name}</span>
                    <span className="metric-value">
                      {(() => {
                        const valObj = point.current_value;
                        const hasValue = valObj !== undefined && valObj !== null;

                        // Extract actual value: check if it's an object with 'value' property or primitive
                        // Some endpoints might return { value: ... } while others might just return the value if structure differs
                        const actualValue = (hasValue && typeof valObj === 'object' && 'value' in valObj)
                          ? valObj.value
                          : valObj;

                        // Check if the extracted value is valid (allow 0, false, but not null/undefined/empty string if desired)
                        const isValid = actualValue !== undefined && actualValue !== null && actualValue !== '';

                        if (isValid) {
                          return (
                            <>
                              <span className="val">{String(actualValue)}</span>
                              {point.unit && point.unit.toLowerCase() !== 'count' && <span className="unit">{point.unit}</span>}
                            </>
                          );
                        } else {
                          return <span className="no-val">-</span>;
                        }
                      })()}
                    </span>
                  </div>
                ))}
              </div>
            ) : (
              <div className="no-data">데이터 포인트 없음</div>
            )}

            {connectionTestResult && (
              <div className={`test-result-box ${connectionTestResult.test_successful ? 'success' : 'fail'}`}>
                <div className="test-header">
                  <i className={`fas ${connectionTestResult.test_successful ? 'fa-check' : 'fa-exclamation-triangle'}`}></i>
                  <span>{connectionTestResult.test_successful ? '테스트 성공' : '테스트 실패'}</span>
                </div>
                {connectionTestResult.response_time_ms && <div className="test-time">{connectionTestResult.response_time_ms}ms</div>}
              </div>
            )}
          </div>
        </div>

      </div>

      <style>{`
        .status-tab-wrapper {
          height: 100%;
          display: flex;
          flex-direction: column;
          padding: 16px;
          background: #f8fafc;
          overflow: hidden;
          box-sizing: border-box;
        }

        .status-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 12px;
          flex-shrink: 0;
        }

        .status-time {
          font-size: 12px;
          color: #64748b;
        }

        .status-actions {
          display: flex;
          gap: 8px;
        }

        /* ACTIONS BTNS */
        .st-btn {
          display: inline-flex;
          align-items: center;
          gap: 6px;
          padding: 6px 12px;
          border-radius: 4px;
          font-size: 12px;
          font-weight: 500;
          cursor: pointer;
          border: none;
          transition: all 0.2s;
        }
        .st-btn-secondary { background: white; border: 1px solid #cbd5e1; color: #475569; }
        .st-btn-secondary:hover { background: #f1f5f9; }
        .st-btn-primary { background: #0ea5e9; color: white; }
        .st-btn-primary:hover { background: #0284c7; }
        .st-btn-accent { background: #8b5cf6; color: white; }
        .st-btn-accent:hover { background: #7c3aed; }
        .st-btn:disabled { opacity: 0.6; cursor: not-allowed; }

        /* GRID LAYOUT - Horizontal */
        .status-grid {
          display: grid;
          grid-template-columns: repeat(4, 1fr);
          gap: 12px;
          flex: 1;
          min-height: 0;
        }

        .status-card {
          background: white;
          border: 1px solid #e2e8f0;
          border-radius: 8px;
          padding: 12px;
          display: flex;
          flex-direction: column;
          gap: 10px;
          box-shadow: 0 1px 2px rgba(0,0,0,0.05);
          overflow: hidden; /* Important for inner scroll */
        }

        .status-card h3 {
          margin: 0;
          font-size: 13px;
          font-weight: 600;
          color: #1e293b;
          display: flex;
          align-items: center;
          gap: 6px;
          padding-bottom: 8px;
          border-bottom: 1px solid #f1f5f9;
          flex-shrink: 0;
        }

        .icon-con { color: #3b82f6; }
        .icon-perf { color: #8b5cf6; }
        .icon-sys { color: #10b981; }
        .icon-data { color: #f59e0b; }

        .status-content {
          display: flex;
          flex-direction: column;
          gap: 6px;
          overflow-y: auto;
        }

        .status-content-scrollable {
          display: flex;
          flex-direction: column;
          gap: 6px;
          overflow-y: auto;
          flex: 1;
          padding-right: 2px; /* Scrollbar space */
          max-height: 300px; /* Explicit max-height for scrolling */
        }

        .status-field {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 6px 8px;
          background: #f8fafc;
          border-radius: 4px;
          border: 1px solid #f1f5f9;
          flex-shrink: 0;
        }

        .field-label {
          font-size: 12px;
          color: #64748b;
          font-weight: 500;
        }

        .field-value {
          font-size: 12px;
          color: #334155;
          font-weight: 600;
        }

        .status-badge-row {
          display: flex;
          align-items: center;
          gap: 4px;
        }

        .text-success-600 { color: #16a34a !important; }
        .text-error-600 { color: #dc2626 !important; }
        .text-warning-600 { color: #d97706 !important; }
        .text-neutral-500 { color: #64748b !important; }

        .error-box {
          background: #fef2f2;
          border: 1px solid #fecaca;
          border-radius: 4px;
          padding: 6px;
          font-size: 11px;
          color: #b91c1c;
        }
        .error-label { font-weight: 600; margin-right: 4px; }

        .diagnose-result {
          margin-top: 10px;
          padding: 10px;
          border-radius: 6px;
          font-size: 11px;
          position: relative;
          word-break: break-all;
          overflow-x: hidden;
        }
        .diagnose-result.success { background: #f0fdf4; border: 1px solid #bbf7d0; color: #166534; }
        .diagnose-result.error { background: #fef2f2; border: 1px solid #fecaca; color: #991b1b; }
        .diag-header { display: flex; align-items: center; gap: 6px; margin-bottom: 6px; }
        .diag-body p { margin: 4px 0; line-height: 1.4; }
        .diag-details { margin: 4px 0; padding-left: 14px; list-style: disc; }
        .diag-details li { margin-bottom: 2px; }
        .diag-close { 
          position: absolute; top: 4px; right: 8px; background: none; border: none; 
          color: inherit; cursor: pointer; font-size: 14px; opacity: 0.5;
        }
        .diag-close:hover { opacity: 1; }

        /* DATA POINTS LIST */
        .metrics-list {
          display: flex;
          flex-direction: column;
          gap: 4px;
        }
        .metric-item-row {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 6px 8px;
          background: #ffffff;
          border-bottom: 1px solid #f1f5f9;
          font-size: 12px;
        }
        .metric-item-row:last-child { border-bottom: none; }
        .metric-name {
          color: #64748b;
          font-weight: 500;
          overflow: hidden;
          text-overflow: ellipsis;
          white-space: nowrap;
          max-width: 60%;
        }
        .metric-value {
          font-weight: 600;
          color: #334155;
          display: flex;
          gap: 2px;
          align-items: center;
        }
        .metric-value .unit {
          color: #94a3b8;
          font-size: 10px;
          font-weight: 400;
        }
        .metric-value .no-val { color: #cbd5e1; }

        .no-data {
           text-align: center;
           color: #94a3b8;
           font-size: 12px;
           padding: 10px;
        }

        .test-result-box {
          margin-top: auto; /* Push to bottom */
          padding: 8px;
          border-radius: 4px;
          font-size: 12px;
          display: flex;
          justify-content: space-between;
          align-items: center;
        }
        .test-result-box.success { background: #f0fdf4; border: 1px solid #bbf7d0; color: #15803d; }
        .test-result-box.fail { background: #fef2f2; border: 1px solid #fecaca; color: #b91c1c; }
        .test-header { display: flex; align-items: center; gap: 6px; font-weight: 600; }

        /* Responsive */
        @media (max-width: 1024px) {
           .status-grid {
              grid-template-columns: repeat(2, 1fr);
           }
        }
        @media (max-width: 640px) {
           .status-grid {
              grid-template-columns: 1fr;
           }
        }
      `}</style>
    </div>
  );
};

export default DeviceStatusTab;