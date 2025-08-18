// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceLogsTab.tsx
// 📝 디바이스 로그 탭 컴포넌트
// ============================================================================

import React, { useState, useEffect } from 'react';
import { DeviceLogsTabProps, DeviceLogEntry } from './types';

const DeviceLogsTab: React.FC<DeviceLogsTabProps> = ({ deviceId }) => {
  const [logs, setLogs] = useState<DeviceLogEntry[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [logLevel, setLogLevel] = useState<string>('all');
  const [autoRefresh, setAutoRefresh] = useState(false);

  // 로그 로드 함수 (실제 API 구현 후 활성화)
  const loadLogs = async () => {
    try {
      setIsLoading(true);
      setError(null);
      
      // TODO: 실제 API 호출로 교체
      // const response = await DeviceApiService.getDeviceLogs(deviceId, {
      //   level: logLevel !== 'all' ? logLevel : undefined,
      //   limit: 100
      // });
      
      // 임시 더미 데이터
      const dummyLogs: DeviceLogEntry[] = [
        {
          id: 1,
          device_id: deviceId,
          timestamp: new Date(Date.now() - 1000 * 60 * 5).toISOString(),
          level: 'INFO',
          category: 'CONNECTION',
          message: '디바이스 연결 성공',
          details: { endpoint: '192.168.1.100:502', response_time: 120 }
        },
        {
          id: 2,
          device_id: deviceId,
          timestamp: new Date(Date.now() - 1000 * 60 * 10).toISOString(),
          level: 'WARN',
          category: 'COMMUNICATION',
          message: '응답 시간이 평균보다 높습니다',
          details: { response_time: 2500, average: 150 }
        },
        {
          id: 3,
          device_id: deviceId,
          timestamp: new Date(Date.now() - 1000 * 60 * 15).toISOString(),
          level: 'ERROR',
          category: 'DATA_READ',
          message: '데이터포인트 읽기 실패',
          details: { point_address: '40001', error: 'Timeout' }
        },
        {
          id: 4,
          device_id: deviceId,
          timestamp: new Date(Date.now() - 1000 * 60 * 20).toISOString(),
          level: 'DEBUG',
          category: 'PROTOCOL',
          message: 'Modbus 요청 전송',
          details: { function_code: 3, address: 1001, count: 10 }
        },
        {
          id: 5,
          device_id: deviceId,
          timestamp: new Date(Date.now() - 1000 * 60 * 25).toISOString(),
          level: 'INFO',
          category: 'CONFIGURATION',
          message: '디바이스 설정 업데이트',
          details: { polling_interval: 5000, timeout: 10000 }
        }
      ];

      // 레벨 필터링
      const filteredLogs = logLevel === 'all' 
        ? dummyLogs 
        : dummyLogs.filter(log => log.level === logLevel);

      setLogs(filteredLogs);
      
    } catch (err) {
      console.error('로그 로드 실패:', err);
      setError(err instanceof Error ? err.message : '로그를 불러올 수 없습니다');
    } finally {
      setIsLoading(false);
    }
  };

  // 로그 내보내기
  const handleExportLogs = () => {
    const csvContent = [
      'Timestamp,Level,Category,Message,Details',
      ...logs.map(log => 
        `"${log.timestamp}","${log.level}","${log.category}","${log.message}","${JSON.stringify(log.details || {})}"`
      )
    ].join('\n');

    const blob = new Blob([csvContent], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `device_${deviceId}_logs_${new Date().toISOString().split('T')[0]}.csv`;
    link.click();
    window.URL.revokeObjectURL(url);
  };

  // 로그 레벨별 색상
  const getLogLevelColor = (level: string) => {
    switch (level) {
      case 'ERROR': return 'log-error';
      case 'WARN': return 'log-warn';
      case 'INFO': return 'log-info';
      case 'DEBUG': return 'log-debug';
      default: return 'log-default';
    }
  };

  // 시간 포맷
  const formatTimestamp = (timestamp: string) => {
    return new Date(timestamp).toLocaleString('ko-KR', {
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  };

  // 초기 로드
  useEffect(() => {
    loadLogs();
  }, [deviceId, logLevel]);

  // 자동 새로고침
  useEffect(() => {
    if (!autoRefresh) return;

    const interval = setInterval(() => {
      loadLogs();
    }, 5000); // 5초마다

    return () => clearInterval(interval);
  }, [autoRefresh, deviceId, logLevel]);

  return (
    <div className="tab-panel">
      <div className="logs-header">
        <h3>디바이스 로그</h3>
        <div className="log-controls">
          <select 
            className="filter-select"
            value={logLevel}
            onChange={(e) => setLogLevel(e.target.value)}
          >
            <option value="all">전체 레벨</option>
            <option value="ERROR">ERROR</option>
            <option value="WARN">WARN</option>
            <option value="INFO">INFO</option>
            <option value="DEBUG">DEBUG</option>
          </select>
          
          <button 
            className={`btn btn-sm ${autoRefresh ? 'btn-warning' : 'btn-secondary'}`}
            onClick={() => setAutoRefresh(!autoRefresh)}
          >
            <i className={`fas fa-${autoRefresh ? 'pause' : 'play'}`}></i>
            {autoRefresh ? '자동새로고침 중지' : '자동새로고침'}
          </button>

          <button 
            className="btn btn-sm btn-secondary"
            onClick={loadLogs}
            disabled={isLoading}
          >
            <i className={`fas fa-sync ${isLoading ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>

          <button 
            className="btn btn-sm btn-primary"
            onClick={handleExportLogs}
            disabled={logs.length === 0}
          >
            <i className="fas fa-download"></i>
            내보내기
          </button>
        </div>
      </div>

      <div className="logs-content">
        {isLoading ? (
          <div className="loading-message">
            <i className="fas fa-spinner fa-spin"></i>
            <p>로그를 불러오는 중...</p>
          </div>
        ) : error ? (
          <div className="error-message">
            <i className="fas fa-exclamation-triangle"></i>
            <p>로그 로드 실패: {error}</p>
            <button className="btn btn-sm btn-primary" onClick={loadLogs}>
              <i className="fas fa-redo"></i>
              다시 시도
            </button>
          </div>
        ) : logs.length > 0 ? (
          <div className="logs-table">
            <table>
              <thead>
                <tr>
                  <th>시간</th>
                  <th>레벨</th>
                  <th>카테고리</th>
                  <th>메시지</th>
                  <th>상세</th>
                </tr>
              </thead>
              <tbody>
                {logs.map((log) => (
                  <tr key={log.id} className={getLogLevelColor(log.level)}>
                    <td className="log-timestamp">
                      {formatTimestamp(log.timestamp)}
                    </td>
                    <td>
                      <span className={`log-level-badge ${getLogLevelColor(log.level)}`}>
                        {log.level}
                      </span>
                    </td>
                    <td className="log-category">{log.category}</td>
                    <td className="log-message">{log.message}</td>
                    <td className="log-details">
                      {log.details ? (
                        <details>
                          <summary>상세보기</summary>
                          <pre>{JSON.stringify(log.details, null, 2)}</pre>
                        </details>
                      ) : (
                        '-'
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        ) : (
          <div className="empty-message">
            <i className="fas fa-file-alt"></i>
            <p>표시할 로그가 없습니다.</p>
            <p>필터 조건을 변경하거나 새로고침을 시도해보세요.</p>
          </div>
        )}
      </div>

      <style jsx>{`
        .tab-panel {
          flex: 1;
          overflow-y: auto;
          padding: 2rem;
          height: 100%;
        }

        .logs-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 1.5rem;
        }

        .logs-header h3 {
          margin: 0;
          font-size: 1.125rem;
          font-weight: 600;
          color: #1f2937;
        }

        .log-controls {
          display: flex;
          gap: 0.5rem;
          align-items: center;
        }

        .filter-select {
          padding: 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
        }

        .logs-content {
          border: 1px solid #e5e7eb;
          border-radius: 0.5rem;
          overflow: hidden;
        }

        .logs-table {
          overflow-x: auto;
        }

        .logs-table table {
          width: 100%;
          border-collapse: collapse;
        }

        .logs-table th {
          background: #f9fafb;
          padding: 0.75rem;
          text-align: left;
          font-size: 0.75rem;
          font-weight: 600;
          color: #374151;
          border-bottom: 1px solid #e5e7eb;
        }

        .logs-table td {
          padding: 0.75rem;
          border-bottom: 1px solid #f3f4f6;
          font-size: 0.875rem;
          vertical-align: top;
        }

        .logs-table tr:last-child td {
          border-bottom: none;
        }

        .log-timestamp {
          font-family: 'Courier New', monospace;
          font-size: 0.75rem;
          color: #6b7280;
          white-space: nowrap;
        }

        .log-level-badge {
          display: inline-block;
          padding: 0.25rem 0.5rem;
          border-radius: 0.25rem;
          font-size: 0.75rem;
          font-weight: 500;
          text-align: center;
          min-width: 60px;
        }

        .log-error {
          background: #fee2e2;
          color: #991b1b;
        }

        .log-warn {
          background: #fef3c7;
          color: #92400e;
        }

        .log-info {
          background: #dbeafe;
          color: #1e40af;
        }

        .log-debug {
          background: #f3f4f6;
          color: #6b7280;
        }

        .log-default {
          background: #f9fafb;
          color: #374151;
        }

        .log-category {
          font-weight: 500;
          color: #374151;
        }

        .log-message {
          color: #1f2937;
        }

        .log-details details {
          cursor: pointer;
        }

        .log-details summary {
          color: #0ea5e9;
          font-size: 0.75rem;
        }

        .log-details pre {
          margin-top: 0.5rem;
          font-size: 0.75rem;
          background: #f9fafb;
          padding: 0.5rem;
          border-radius: 0.25rem;
          border: 1px solid #e5e7eb;
          overflow-x: auto;
        }

        .loading-message,
        .error-message {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          min-height: 300px;
          gap: 1rem;
          padding: 2rem;
          text-align: center;
        }

        .loading-message i {
          font-size: 2rem;
          color: #0ea5e9;
        }

        .error-message i {
          font-size: 2rem;
          color: #dc2626;
        }

        .loading-message p,
        .error-message p {
          margin: 0;
          color: #6b7280;
          font-size: 0.875rem;
        }

        .empty-message {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          min-height: 300px;
          color: #6b7280;
          text-align: center;
          gap: 1rem;
          padding: 2rem;
        }

        .empty-message i {
          font-size: 3rem;
          color: #cbd5e1;
        }

        .empty-message p {
          margin: 0;
          font-size: 0.875rem;
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

        .btn-primary {
          background: #0ea5e9;
          color: white;
        }

        .btn-secondary {
          background: #64748b;
          color: white;
        }

        .btn-warning {
          background: #f59e0b;
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

export default DeviceLogsTab;