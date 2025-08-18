// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceStatusTab.tsx
// 📈 디바이스 상태 탭 컴포넌트 - 완전 구현
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceApiService } from '../../../api/services/deviceApi';
import { DataApiService } from '../../../api/services/dataApi';
import { DeviceStatusTabProps } from './types';

const DeviceStatusTab: React.FC<DeviceStatusTabProps> = ({ device }) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [isTestingConnection, setIsTestingConnection] = useState(false);
  const [connectionTestResult, setConnectionTestResult] = useState<any>(null);
  const [deviceStats, setDeviceStats] = useState<any>(null);
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
   * 디바이스 통계 로드
   */
  const loadDeviceStats = async () => {
    if (!device?.id) return;

    try {
      setIsLoadingStats(true);
      
      // 디바이스별 현재값 조회
      const currentValuesResponse = await DataApiService.getDeviceCurrentValues(device.id, {
        include_metadata: true,
        include_trends: true
      });

      if (currentValuesResponse.success && currentValuesResponse.data) {
        setDeviceStats(currentValuesResponse.data);
      }
    } catch (error) {
      console.error('디바이스 통계 로드 실패:', error);
    } finally {
      setIsLoadingStats(false);
    }
  };

  /**
   * 상태 새로고침
   */
  const handleRefreshStatus = () => {
    setLastUpdate(new Date());
    loadDeviceStats();
  };

  // ========================================================================
  // 라이프사이클
  // ========================================================================

  useEffect(() => {
    if (device?.id) {
      loadDeviceStats();
    }
  }, [device?.id]);

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
   * 상태 아이콘 함수
   */
  const getStatusIcon = (status?: string) => {
    switch (status) {
      case 'connected': return 'fa-check-circle';
      case 'disconnected': return 'fa-times-circle';
      case 'connecting': return 'fa-spinner fa-spin';
      case 'error': return 'fa-exclamation-circle';
      default: return 'fa-question-circle';
    }
  };

  /**
   * 시간 포맷 함수
   */
  const formatTimeAgo = (dateString?: string) => {
    if (!dateString) return 'N/A';
    const diff = Math.floor((Date.now() - new Date(dateString).getTime()) / 60000);
    if (diff < 1) return '방금 전';
    if (diff < 60) return `${diff}분 전`;
    if (diff < 1440) return `${Math.floor(diff/60)}시간 전`;
    return `${Math.floor(diff/1440)}일 전`;
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
   * 처리량 포맷
   */
  const formatThroughput = (value?: number) => {
    if (!value) return 'N/A';
    return `${value.toFixed(2)} ops/sec`;
  };

  // ========================================================================
  // 렌더링
  // ========================================================================

  return (
    <div className="tab-panel">
      <div className="status-container">
        
        {/* 헤더 */}
        <div className="status-header">
          <div className="header-left">
            <h3>📈 디바이스 상태</h3>
            <span className="last-update">
              마지막 업데이트: {lastUpdate.toLocaleTimeString()}
            </span>
          </div>
          <div className="header-right">
            <button
              className="btn btn-secondary btn-sm"
              onClick={handleRefreshStatus}
              disabled={isLoadingStats}
            >
              {isLoadingStats ? (
                <i className="fas fa-spinner fa-spin"></i>
              ) : (
                <i className="fas fa-sync"></i>
              )}
              새로고침
            </button>
            <button
              className="btn btn-info btn-sm"
              onClick={handleTestConnection}
              disabled={isTestingConnection}
            >
              {isTestingConnection ? (
                <>
                  <i className="fas fa-spinner fa-spin"></i>
                  테스트 중...
                </>
              ) : (
                <>
                  <i className="fas fa-plug"></i>
                  연결 테스트
                </>
              )}
            </button>
          </div>
        </div>

        <div className="status-grid">
          
          {/* 연결 상태 카드 */}
          <div className="status-card">
            <div className="card-header">
              <h4>🔗 연결 상태</h4>
            </div>
            <div className="card-content">
              <div className="status-item">
                <span className="label">현재 상태:</span>
                <span className={`status-value ${getStatusColor(displayData?.connection_status)}`}>
                  <i className={`fas ${getStatusIcon(displayData?.connection_status)}`}></i>
                  {displayData?.connection_status === 'connected' ? '연결됨' :
                   displayData?.connection_status === 'disconnected' ? '연결끊김' :
                   displayData?.connection_status === 'connecting' ? '연결중' :
                   displayData?.connection_status === 'error' ? '오류' : '알수없음'}
                </span>
              </div>

              <div className="status-item">
                <span className="label">마지막 통신:</span>
                <span className="value">
                  {formatTimeAgo(statusInfo.last_communication || displayData?.last_seen)}
                </span>
              </div>

              <div className="status-item">
                <span className="label">응답 시간:</span>
                <span className="value">
                  {statusInfo.response_time ? `${statusInfo.response_time}ms` : 'N/A'}
                </span>
              </div>

              <div className="status-item">
                <span className="label">오류 횟수:</span>
                <span className={`value ${(statusInfo.error_count || 0) > 0 ? 'text-error-600' : ''}`}>
                  {statusInfo.error_count || 0}회
                </span>
              </div>

              {statusInfo.last_error && (
                <div className="status-item">
                  <span className="label">마지막 오류:</span>
                  <span className="value error-message">{statusInfo.last_error}</span>
                </div>
              )}
            </div>
          </div>

          {/* 성능 지표 카드 */}
          <div className="status-card">
            <div className="card-header">
              <h4>⚡ 성능 지표</h4>
            </div>
            <div className="card-content">
              <div className="status-item">
                <span className="label">총 요청:</span>
                <span className="value">{(statusInfo.total_requests || 0).toLocaleString()}회</span>
              </div>

              <div className="status-item">
                <span className="label">성공 요청:</span>
                <span className="value text-success-600">
                  {(statusInfo.successful_requests || 0).toLocaleString()}회
                </span>
              </div>

              <div className="status-item">
                <span className="label">실패 요청:</span>
                <span className="value text-error-600">
                  {(statusInfo.failed_requests || 0).toLocaleString()}회
                </span>
              </div>

              <div className="status-item">
                <span className="label">성공률:</span>
                <span className={`value ${parseFloat(calculateSuccessRate()) >= 95 ? 'text-success-600' : 
                                        parseFloat(calculateSuccessRate()) >= 90 ? 'text-warning-600' : 'text-error-600'}`}>
                  {calculateSuccessRate()}%
                </span>
              </div>

              <div className="status-item">
                <span className="label">처리량:</span>
                <span className="value">
                  {formatThroughput(statusInfo.throughput_ops_per_sec)}
                </span>
              </div>
            </div>
          </div>

          {/* 디바이스 정보 카드 */}
          <div className="status-card">
            <div className="card-header">
              <h4>🔧 디바이스 정보</h4>
            </div>
            <div className="card-content">
              {statusInfo.firmware_version && (
                <div className="status-item">
                  <span className="label">펌웨어 버전:</span>
                  <span className="value">{statusInfo.firmware_version}</span>
                </div>
              )}

              {statusInfo.hardware_info && (
                <div className="status-item">
                  <span className="label">하드웨어 정보:</span>
                  <span className="value">{statusInfo.hardware_info}</span>
                </div>
              )}

              {statusInfo.cpu_usage !== undefined && (
                <div className="status-item">
                  <span className="label">CPU 사용률:</span>
                  <span className={`value ${statusInfo.cpu_usage > 80 ? 'text-error-600' : 
                                           statusInfo.cpu_usage > 60 ? 'text-warning-600' : 'text-success-600'}`}>
                    {statusInfo.cpu_usage}%
                  </span>
                </div>
              )}

              {statusInfo.memory_usage !== undefined && (
                <div className="status-item">
                  <span className="label">메모리 사용률:</span>
                  <span className={`value ${statusInfo.memory_usage > 80 ? 'text-error-600' : 
                                           statusInfo.memory_usage > 60 ? 'text-warning-600' : 'text-success-600'}`}>
                    {statusInfo.memory_usage}%
                  </span>
                </div>
              )}

              {statusInfo.uptime_percentage !== undefined && (
                <div className="status-item">
                  <span className="label">가동률:</span>
                  <span className={`value ${statusInfo.uptime_percentage >= 99 ? 'text-success-600' : 
                                           statusInfo.uptime_percentage >= 95 ? 'text-warning-600' : 'text-error-600'}`}>
                    {statusInfo.uptime_percentage}%
                  </span>
                </div>
              )}
            </div>
          </div>

          {/* 데이터포인트 통계 카드 */}
          {deviceStats && (
            <div className="status-card">
              <div className="card-header">
                <h4>📊 데이터포인트 통계</h4>
              </div>
              <div className="card-content">
                <div className="status-item">
                  <span className="label">총 포인트:</span>
                  <span className="value">{deviceStats.total_points || 0}개</span>
                </div>

                <div className="status-item">
                  <span className="label">정상 품질:</span>
                  <span className="value text-success-600">
                    {deviceStats.summary?.good_quality || 0}개
                  </span>
                </div>

                <div className="status-item">
                  <span className="label">불량 품질:</span>
                  <span className="value text-error-600">
                    {deviceStats.summary?.bad_quality || 0}개
                  </span>
                </div>

                <div className="status-item">
                  <span className="label">불확실 품질:</span>
                  <span className="value text-warning-600">
                    {deviceStats.summary?.uncertain_quality || 0}개
                  </span>
                </div>

                <div className="status-item">
                  <span className="label">마지막 업데이트:</span>
                  <span className="value">
                    {deviceStats.summary?.last_update ? 
                     formatTimeAgo(new Date(deviceStats.summary.last_update).toISOString()) : 'N/A'}
                  </span>
                </div>
              </div>
            </div>
          )}

          {/* 연결 테스트 결과 카드 */}
          {connectionTestResult && (
            <div className="status-card">
              <div className="card-header">
                <h4>🔍 연결 테스트 결과</h4>
              </div>
              <div className="card-content">
                <div className="status-item">
                  <span className="label">테스트 결과:</span>
                  <span className={`status-value ${connectionTestResult.test_successful ? 
                                   'text-success-600' : 'text-error-600'}`}>
                    <i className={`fas ${connectionTestResult.test_successful ? 
                                  'fa-check-circle' : 'fa-times-circle'}`}></i>
                    {connectionTestResult.test_successful ? '성공' : '실패'}
                  </span>
                </div>

                {connectionTestResult.response_time_ms && (
                  <div className="status-item">
                    <span className="label">응답 시간:</span>
                    <span className="value">{connectionTestResult.response_time_ms}ms</span>
                  </div>
                )}

                {connectionTestResult.test_timestamp && (
                  <div className="status-item">
                    <span className="label">테스트 시간:</span>
                    <span className="value">
                      {new Date(connectionTestResult.test_timestamp).toLocaleString()}
                    </span>
                  </div>
                )}

                {connectionTestResult.error_message && (
                  <div className="status-item">
                    <span className="label">오류 메시지:</span>
                    <span className="value error-message">{connectionTestResult.error_message}</span>
                  </div>
                )}
              </div>
            </div>
          )}

          {/* 진단 정보 카드 */}
          {statusInfo.diagnostic_data && (
            <div className="status-card span-full">
              <div className="card-header">
                <h4>🔧 진단 정보</h4>
              </div>
              <div className="card-content">
                <pre className="diagnostic-data">
                  {JSON.stringify(statusInfo.diagnostic_data, null, 2)}
                </pre>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* 스타일 */}
      <style jsx>{`
        .tab-panel {
          flex: 1;
          padding: 1.5rem;
          overflow-y: auto;
          background: #f8fafc;
        }

        .status-container {
          display: flex;
          flex-direction: column;
          gap: 1.5rem;
        }

        .status-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          background: white;
          padding: 1rem 1.5rem;
          border-radius: 0.5rem;
          border: 1px solid #e5e7eb;
        }

        .header-left h3 {
          margin: 0;
          font-size: 1.125rem;
          font-weight: 600;
          color: #1f2937;
        }

        .last-update {
          font-size: 0.75rem;
          color: #6b7280;
          margin-top: 0.25rem;
        }

        .header-right {
          display: flex;
          gap: 0.75rem;
        }

        .status-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
          gap: 1.5rem;
        }

        .status-card {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          overflow: hidden;
        }

        .status-card.span-full {
          grid-column: 1 / -1;
        }

        .card-header {
          background: #f9fafb;
          padding: 1rem 1.5rem;
          border-bottom: 1px solid #e5e7eb;
        }

        .card-header h4 {
          margin: 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .card-content {
          padding: 1.5rem;
          display: flex;
          flex-direction: column;
          gap: 1rem;
        }

        .status-item {
          display: flex;
          justify-content: space-between;
          align-items: flex-start;
          gap: 1rem;
        }

        .status-item .label {
          font-size: 0.875rem;
          font-weight: 500;
          color: #6b7280;
          min-width: 120px;
        }

        .status-item .value,
        .status-item .status-value {
          font-size: 0.875rem;
          font-weight: 500;
          color: #374151;
          text-align: right;
          flex: 1;
        }

        .status-value {
          display: flex;
          align-items: center;
          justify-content: flex-end;
          gap: 0.5rem;
        }

        .error-message {
          font-family: 'Courier New', monospace;
          font-size: 0.75rem;
          background: #fef2f2;
          padding: 0.5rem;
          border-radius: 0.25rem;
          border: 1px solid #fecaca;
          color: #dc2626;
          word-break: break-all;
        }

        .diagnostic-data {
          background: #f3f4f6;
          padding: 1rem;
          border-radius: 0.375rem;
          font-size: 0.75rem;
          line-height: 1.4;
          overflow-x: auto;
          white-space: pre-wrap;
          color: #374151;
          border: 1px solid #d1d5db;
        }

        .text-success-600 {
          color: #059669 !important;
        }

        .text-warning-600 {
          color: #d97706 !important;
        }

        .text-error-600 {
          color: #dc2626 !important;
        }

        .text-neutral-500 {
          color: #6b7280 !important;
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

        .btn-sm {
          padding: 0.375rem 0.75rem;
          font-size: 0.75rem;
        }

        .btn-secondary {
          background: #64748b;
          color: white;
        }

        .btn-secondary:hover:not(:disabled) {
          background: #475569;
        }

        .btn-info {
          background: #0891b2;
          color: white;
        }

        .btn-info:hover:not(:disabled) {
          background: #0e7490;
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }
      `}</style>
    </div>
  );
};

export default DeviceStatusTab;