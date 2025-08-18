// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceStatusTab.tsx
// 📈 디바이스 상태 탭 컴포넌트
// ============================================================================

import React from 'react';
import { DeviceStatusTabProps } from './types';

const DeviceStatusTab: React.FC<DeviceStatusTabProps> = ({ device }) => {
  const displayData = device;

  // 상태 색상 함수
  const getStatusColor = (status?: string) => {
    switch (status) {
      case 'connected': return 'text-success-600';
      case 'disconnected': return 'text-error-600';
      case 'connecting': return 'text-warning-600';
      case 'error': return 'text-error-600';
      default: return 'text-neutral-500';
    }
  };

  // 시간 포맷 함수
  const formatTimeAgo = (dateString?: string) => {
    if (!dateString) return 'N/A';
    const diff = Math.floor((Date.now() - new Date(dateString).getTime()) / 60000);
    return diff < 1 ? '방금 전' : diff < 60 ? `${diff}분 전` : `${Math.floor(diff/60)}시간 전`;
  };

  return (
    <div className="tab-panel">
      <div className="status-grid">
        <div className="status-card">
          <h4>연결 상태</h4>
          <div className="status-details">
            <div className="status-item">
              <span className="label">현재 상태:</span>
              <span className={`value ${getStatusColor(displayData?.status?.connection_status)}`}>
                {displayData?.status?.connection_status === 'connected' ? '연결됨' :
                 displayData?.status?.connection_status === 'disconnected' ? '연결끊김' :
                 displayData?.status?.connection_status === 'connecting' ? '연결중' : '알수없음'}
              </span>
            </div>
            <div className="status-item">
              <span className="label">마지막 통신:</span>
              <span className="value">{formatTimeAgo(displayData?.status?.last_communication)}</span>
            </div>
            <div className="status-item">
              <span className="label">응답시간:</span>
              <span className="value">{displayData?.status?.response_time || 0}ms</span>
            </div>
            <div className="status-item">
              <span className="label">가동률:</span>
              <span className="value">{displayData?.status?.uptime_percentage || 0}%</span>
            </div>
          </div>
        </div>

        <div className="status-card">
          <h4>통신 통계</h4>
          <div className="status-details">
            <div className="status-item">
              <span className="label">총 요청:</span>
              <span className="value">{displayData?.status?.total_requests || 0}</span>
            </div>
            <div className="status-item">
              <span className="label">성공:</span>
              <span className="value text-success-600">{displayData?.status?.successful_requests || 0}</span>
            </div>
            <div className="status-item">
              <span className="label">실패:</span>
              <span className="value text-error-600">{displayData?.status?.failed_requests || 0}</span>
            </div>
            <div className="status-item">
              <span className="label">처리량:</span>
              <span className="value">{displayData?.status?.throughput_ops_per_sec || 0} ops/sec</span>
            </div>
          </div>
        </div>

        <div className="status-card">
          <h4>오류 정보</h4>
          <div className="status-details">
            <div className="status-item">
              <span className="label">오류 횟수:</span>
              <span className={`value ${(displayData?.status?.error_count || 0) > 0 ? 'text-error-600' : ''}`}>
                {displayData?.status?.error_count || 0}
              </span>
            </div>
            <div className="status-item full-width">
              <span className="label">마지막 오류:</span>
              <span className="value error-message">
                {displayData?.status?.last_error || '오류 없음'}
              </span>
            </div>
          </div>
        </div>

        <div className="status-card">
          <h4>하드웨어 정보</h4>
          <div className="status-details">
            <div className="status-item">
              <span className="label">펌웨어 버전:</span>
              <span className="value">{displayData?.status?.firmware_version || 'N/A'}</span>
            </div>
            <div className="status-item">
              <span className="label">CPU 사용률:</span>
              <span className="value">{displayData?.status?.cpu_usage || 0}%</span>
            </div>
            <div className="status-item">
              <span className="label">메모리 사용률:</span>
              <span className="value">{displayData?.status?.memory_usage || 0}%</span>
            </div>
          </div>
        </div>

        <div className="status-card">
          <h4>성능 지표</h4>
          <div className="status-details">
            <div className="status-item">
              <span className="label">평균 응답시간:</span>
              <span className="value">{displayData?.status?.response_time || 0}ms</span>
            </div>
            <div className="status-item">
              <span className="label">성공률:</span>
              <span className="value">
                {displayData?.status?.total_requests && displayData?.status?.successful_requests ? 
                 Math.round((displayData.status.successful_requests / displayData.status.total_requests) * 100) : 
                 0}%
              </span>
            </div>
            <div className="status-item">
              <span className="label">데이터 처리량:</span>
              <span className="value">{displayData?.status?.throughput_ops_per_sec || 0} ops/sec</span>
            </div>
            <div className="status-item">
              <span className="label">시스템 가동률:</span>
              <span className="value">{displayData?.status?.uptime_percentage || 0}%</span>
            </div>
          </div>
        </div>

        <div className="status-card">
          <h4>진단 데이터</h4>
          <div className="status-details">
            {displayData?.status?.diagnostic_data ? (
              Object.entries(displayData.status.diagnostic_data).map(([key, value]) => (
                <div key={key} className="status-item">
                  <span className="label">{key}:</span>
                  <span className="value">{String(value)}</span>
                </div>
              ))
            ) : (
              <div className="status-item">
                <span className="label">진단 데이터:</span>
                <span className="value">사용 가능한 데이터 없음</span>
              </div>
            )}
          </div>
        </div>
      </div>

      <style jsx>{`
        .tab-panel {
          flex: 1;
          overflow-y: auto;
          padding: 2rem;
          height: 100%;
        }

        .status-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
          gap: 1.5rem;
        }

        .status-card {
          border: 1px solid #e5e7eb;
          border-radius: 0.75rem;
          padding: 1.5rem;
          background: white;
        }

        .status-card h4 {
          margin: 0 0 1rem 0;
          font-size: 1rem;
          font-weight: 600;
          color: #1f2937;
        }

        .status-details {
          display: flex;
          flex-direction: column;
          gap: 0.75rem;
        }

        .status-item {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 0.5rem 0;
          border-bottom: 1px solid #f3f4f6;
        }

        .status-item:last-child {
          border-bottom: none;
        }

        .status-item.full-width {
          flex-direction: column;
          align-items: flex-start;
          gap: 0.25rem;
        }

        .status-item .label {
          font-size: 0.75rem;
          color: #6b7280;
          font-weight: 500;
        }

        .status-item .value {
          font-size: 0.875rem;
          color: #1f2937;
          font-weight: 500;
        }

        .error-message {
          font-family: 'Courier New', monospace;
          background: #fef2f2;
          padding: 0.25rem 0.5rem;
          border-radius: 0.25rem;
          color: #991b1b;
          word-break: break-all;
          font-size: 0.75rem;
        }

        .text-success-600 { color: #059669; }
        .text-warning-600 { color: #d97706; }
        .text-error-600 { color: #dc2626; }
        .text-neutral-500 { color: #6b7280; }
      `}</style>
    </div>
  );
};

export default DeviceStatusTab;