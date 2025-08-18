// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceLogsTab.tsx
// 📄 디바이스 로그 탭 컴포넌트 - 완전 구현
// ============================================================================

import React, { useState, useEffect, useRef } from 'react';
import { DeviceLogsTabProps, DeviceLogEntry } from './types';

const DeviceLogsTab: React.FC<DeviceLogsTabProps> = ({ deviceId }) => {
  // ========================================================================
  // 상태 관리
  // ========================================================================
  const [logs, setLogs] = useState<DeviceLogEntry[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [filterLevel, setFilterLevel] = useState<string>('ALL');
  const [filterCategory, setFilterCategory] = useState<string>('ALL');
  const [searchTerm, setSearchTerm] = useState('');
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [refreshInterval, setRefreshInterval] = useState(5); // 초
  const [isRealTime, setIsRealTime] = useState(false);
  const [lastRefresh, setLastRefresh] = useState<Date>(new Date());

  const logsEndRef = useRef<HTMLDivElement>(null);
  const intervalRef = useRef<NodeJS.Timeout | null>(null);

  // ========================================================================
  // Mock 데이터 (실제 API 대신 사용)
  // ========================================================================
  const generateMockLogs = (): DeviceLogEntry[] => {
    const levels = ['DEBUG', 'INFO', 'WARN', 'ERROR'] as const;
    const categories = ['COMMUNICATION', 'DATA_COLLECTION', 'CONNECTION', 'SYSTEM', 'PROTOCOL'];
    const messages = [
      'Successfully connected to device',
      'Data point read completed',
      'Connection timeout occurred',
      'Invalid response received',
      'Polling cycle started',
      'Error parsing data frame',
      'Connection established',
      'Heartbeat received',
      'Data validation failed',
      'Protocol handshake completed',
      'Memory usage warning',
      'Performance metrics updated',
      'Configuration loaded',
      'Diagnostic mode enabled'
    ];

    return Array.from({ length: 100 }, (_, i) => {
      const timestamp = new Date(Date.now() - (i * 30000 + Math.random() * 30000));
      const level = levels[Math.floor(Math.random() * levels.length)];
      const category = categories[Math.floor(Math.random() * categories.length)];
      const message = messages[Math.floor(Math.random() * messages.length)];
      
      return {
        id: Date.now() - i,
        device_id: deviceId,
        timestamp: timestamp.toISOString(),
        level,
        category,
        message,
        details: level === 'ERROR' ? { 
          error_code: Math.floor(Math.random() * 1000),
          stack_trace: 'Error occurred in communication module'
        } : undefined
      };
    }).sort((a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime());
  };

  // ========================================================================
  // API 호출 함수들
  // ========================================================================

  /**
   * 로그 데이터 로드
   */
  const loadLogs = async (showLoading = true) => {
    try {
      if (showLoading) setIsLoading(true);
      setError(null);

      // 실제 API 호출 대신 Mock 데이터 사용
      // const response = await DeviceApiService.getDeviceLogs(deviceId, {
      //   page: 1,
      //   limit: 100,
      //   level: filterLevel !== 'ALL' ? filterLevel : undefined,
      //   category: filterCategory !== 'ALL' ? filterCategory : undefined,
      //   search: searchTerm || undefined
      // });

      await new Promise(resolve => setTimeout(resolve, 500)); // 로딩 시뮬레이션
      const mockLogs = generateMockLogs();
      setLogs(mockLogs);
      setLastRefresh(new Date());

    } catch (error) {
      console.error('로그 로드 실패:', error);
      setError(error instanceof Error ? error.message : 'Unknown error');
    } finally {
      if (showLoading) setIsLoading(false);
    }
  };

  /**
   * 로그 내보내기
   */
  const exportLogs = () => {
    try {
      const filteredLogs = getFilteredLogs();
      const csvContent = [
        ['시간', '레벨', '카테고리', '메시지'].join(','),
        ...filteredLogs.map(log => [
          new Date(log.timestamp).toLocaleString(),
          log.level,
          log.category,
          `"${log.message.replace(/"/g, '""')}"`
        ].join(','))
      ].join('\n');

      const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
      const link = document.createElement('a');
      link.href = URL.createObjectURL(blob);
      link.download = `device_${deviceId}_logs_${new Date().toISOString().split('T')[0]}.csv`;
      link.click();
    } catch (error) {
      console.error('로그 내보내기 실패:', error);
      alert('로그 내보내기에 실패했습니다.');
    }
  };

  /**
   * 로그 클리어
   */
  const clearLogs = () => {
    if (confirm('모든 로그를 삭제하시겠습니까? 이 작업은 되돌릴 수 없습니다.')) {
      setLogs([]);
      alert('로그가 삭제되었습니다.');
    }
  };

  // ========================================================================
  // 필터링 및 검색
  // ========================================================================

  /**
   * 필터링된 로그 가져오기
   */
  const getFilteredLogs = () => {
    return logs.filter(log => {
      const matchesLevel = filterLevel === 'ALL' || log.level === filterLevel;
      const matchesCategory = filterCategory === 'ALL' || log.category === filterCategory;
      const matchesSearch = !searchTerm || 
        log.message.toLowerCase().includes(searchTerm.toLowerCase()) ||
        log.category.toLowerCase().includes(searchTerm.toLowerCase());

      return matchesLevel && matchesCategory && matchesSearch;
    });
  };

  // ========================================================================
  // 자동 새로고침
  // ========================================================================

  useEffect(() => {
    if (autoRefresh && refreshInterval > 0) {
      intervalRef.current = setInterval(() => {
        loadLogs(false); // 백그라운드 새로고침
      }, refreshInterval * 1000);

      return () => {
        if (intervalRef.current) {
          clearInterval(intervalRef.current);
        }
      };
    }
  }, [autoRefresh, refreshInterval, deviceId, filterLevel, filterCategory, searchTerm]);

  // ========================================================================
  // 라이프사이클
  // ========================================================================

  useEffect(() => {
    loadLogs();
  }, [deviceId]);

  useEffect(() => {
    if (isRealTime) {
      logsEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logs, isRealTime]);

  // ========================================================================
  // 렌더링 헬퍼 함수들
  // ========================================================================

  /**
   * 로그 레벨 색상 가져오기
   */
  const getLevelColor = (level: string) => {
    switch (level) {
      case 'DEBUG': return 'level-debug';
      case 'INFO': return 'level-info';
      case 'WARN': return 'level-warn';
      case 'ERROR': return 'level-error';
      default: return 'level-default';
    }
  };

  /**
   * 로그 레벨 아이콘 가져오기
   */
  const getLevelIcon = (level: string) => {
    switch (level) {
      case 'DEBUG': return 'fa-bug';
      case 'INFO': return 'fa-info-circle';
      case 'WARN': return 'fa-exclamation-triangle';
      case 'ERROR': return 'fa-times-circle';
      default: return 'fa-circle';
    }
  };

  /**
   * 로그 항목 렌더링
   */
  const renderLogEntry = (log: DeviceLogEntry, index: number) => (
    <div key={log.id} className={`log-entry ${getLevelColor(log.level)}`}>
      <div className="log-header">
        <div className="log-timestamp">
          {new Date(log.timestamp).toLocaleString()}
        </div>
        <div className="log-level">
          <i className={`fas ${getLevelIcon(log.level)}`}></i>
          {log.level}
        </div>
        <div className="log-category">{log.category}</div>
      </div>
      <div className="log-message">{log.message}</div>
      {log.details && (
        <div className="log-details">
          <details>
            <summary>상세 정보</summary>
            <pre>{JSON.stringify(log.details, null, 2)}</pre>
          </details>
        </div>
      )}
    </div>
  );

  const filteredLogs = getFilteredLogs();

  // ========================================================================
  // 메인 렌더링
  // ========================================================================

  return (
    <div className="tab-panel">
      <div className="logs-container">
        
        {/* 헤더 */}
        <div className="logs-header">
          <div className="header-left">
            <h3>📄 디바이스 로그</h3>
            <span className="log-count">{filteredLogs.length}개 로그</span>
            <span className="last-refresh">
              마지막 업데이트: {lastRefresh.toLocaleTimeString()}
            </span>
          </div>
          <div className="header-right">
            <button
              className="btn btn-secondary btn-sm"
              onClick={() => loadLogs()}
              disabled={isLoading}
            >
              {isLoading ? (
                <i className="fas fa-spinner fa-spin"></i>
              ) : (
                <i className="fas fa-sync"></i>
              )}
              새로고침
            </button>
            <button
              className="btn btn-info btn-sm"
              onClick={exportLogs}
              disabled={filteredLogs.length === 0}
            >
              <i className="fas fa-download"></i>
              내보내기
            </button>
            <button
              className="btn btn-error btn-sm"
              onClick={clearLogs}
              disabled={logs.length === 0}
            >
              <i className="fas fa-trash"></i>
              삭제
            </button>
          </div>
        </div>

        {/* 필터 및 제어 */}
        <div className="logs-controls">
          <div className="filters">
            <div className="search-box">
              <input
                type="text"
                placeholder="메시지 또는 카테고리로 검색..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
              />
              <i className="fas fa-search"></i>
            </div>

            <select
              value={filterLevel}
              onChange={(e) => setFilterLevel(e.target.value)}
            >
              <option value="ALL">전체 레벨</option>
              <option value="DEBUG">DEBUG</option>
              <option value="INFO">INFO</option>
              <option value="WARN">WARN</option>
              <option value="ERROR">ERROR</option>
            </select>

            <select
              value={filterCategory}
              onChange={(e) => setFilterCategory(e.target.value)}
            >
              <option value="ALL">전체 카테고리</option>
              <option value="COMMUNICATION">통신</option>
              <option value="DATA_COLLECTION">데이터 수집</option>
              <option value="CONNECTION">연결</option>
              <option value="SYSTEM">시스템</option>
              <option value="PROTOCOL">프로토콜</option>
            </select>
          </div>

          <div className="auto-refresh">
            <label className="checkbox-label">
              <input
                type="checkbox"
                checked={autoRefresh}
                onChange={(e) => setAutoRefresh(e.target.checked)}
              />
              자동 새로고침
            </label>
            {autoRefresh && (
              <select
                value={refreshInterval}
                onChange={(e) => setRefreshInterval(parseInt(e.target.value))}
              >
                <option value={5}>5초</option>
                <option value={10}>10초</option>
                <option value={30}>30초</option>
                <option value={60}>1분</option>
              </select>
            )}
            <label className="checkbox-label">
              <input
                type="checkbox"
                checked={isRealTime}
                onChange={(e) => setIsRealTime(e.target.checked)}
              />
              실시간 스크롤
            </label>
          </div>
        </div>

        {/* 에러 메시지 */}
        {error && (
          <div className="error-message">
            <i className="fas fa-exclamation-triangle"></i>
            {error}
          </div>
        )}

        {/* 로그 목록 */}
        <div className="logs-content">
          {isLoading && logs.length === 0 ? (
            <div className="loading">
              <i className="fas fa-spinner fa-spin"></i>
              로그를 불러오는 중...
            </div>
          ) : filteredLogs.length === 0 ? (
            <div className="no-logs">
              {logs.length === 0 ? '로그가 없습니다.' : '검색 결과가 없습니다.'}
            </div>
          ) : (
            <div className="logs-list">
              {filteredLogs.map(renderLogEntry)}
              <div ref={logsEndRef} />
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

        .logs-container {
          display: flex;
          flex-direction: column;
          gap: 1rem;
          height: 100%;
        }

        .logs-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          background: white;
          padding: 1rem 1.5rem;
          border-radius: 0.5rem;
          border: 1px solid #e5e7eb;
        }

        .header-left {
          display: flex;
          align-items: center;
          gap: 1rem;
        }

        .header-left h3 {
          margin: 0;
          font-size: 1.125rem;
          font-weight: 600;
          color: #1f2937;
        }

        .log-count {
          background: #0ea5e9;
          color: white;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-size: 0.75rem;
          font-weight: 500;
        }

        .last-refresh {
          font-size: 0.75rem;
          color: #6b7280;
        }

        .header-right {
          display: flex;
          gap: 0.75rem;
        }

        .logs-controls {
          display: flex;
          justify-content: space-between;
          align-items: center;
          background: white;
          padding: 1rem 1.5rem;
          border-radius: 0.5rem;
          border: 1px solid #e5e7eb;
          flex-wrap: wrap;
          gap: 1rem;
        }

        .filters {
          display: flex;
          gap: 1rem;
          flex: 1;
        }

        .search-box {
          position: relative;
          flex: 1;
          max-width: 300px;
        }

        .search-box input {
          width: 100%;
          padding: 0.5rem 2.5rem 0.5rem 0.75rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
        }

        .search-box i {
          position: absolute;
          right: 0.75rem;
          top: 50%;
          transform: translateY(-50%);
          color: #6b7280;
        }

        .filters select {
          padding: 0.5rem 0.75rem;
          border: 1px solid #d1d5db;
          border-radius: 0.375rem;
          font-size: 0.875rem;
          background: white;
        }

        .auto-refresh {
          display: flex;
          align-items: center;
          gap: 0.75rem;
        }

        .checkbox-label {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          font-size: 0.875rem;
          font-weight: 500;
          color: #374151;
          cursor: pointer;
          white-space: nowrap;
        }

        .checkbox-label input {
          margin: 0;
        }

        .error-message {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          background: #fef2f2;
          color: #dc2626;
          padding: 1rem 1.5rem;
          border-radius: 0.5rem;
          border: 1px solid #fecaca;
        }

        .logs-content {
          flex: 1;
          background: white;
          border-radius: 0.5rem;
          border: 1px solid #e5e7eb;
          overflow: hidden;
          display: flex;
          flex-direction: column;
        }

        .loading,
        .no-logs {
          display: flex;
          align-items: center;
          justify-content: center;
          gap: 0.5rem;
          padding: 2rem;
          color: #6b7280;
          flex: 1;
        }

        .logs-list {
          flex: 1;
          overflow-y: auto;
          padding: 1rem;
          display: flex;
          flex-direction: column;
          gap: 0.5rem;
        }

        .log-entry {
          border: 1px solid #e5e7eb;
          border-radius: 0.375rem;
          padding: 1rem;
          background: #fafafa;
          transition: all 0.2s ease;
          animation: fadeIn 0.3s ease-in-out;
        }

        .log-entry:hover {
          background: #f3f4f6;
          border-color: #d1d5db;
        }

        .log-entry.level-debug {
          border-left: 4px solid #6b7280;
        }

        .log-entry.level-info {
          border-left: 4px solid #0ea5e9;
        }

        .log-entry.level-warn {
          border-left: 4px solid #f59e0b;
        }

        .log-entry.level-error {
          border-left: 4px solid #dc2626;
        }

        .log-header {
          display: flex;
          align-items: center;
          gap: 1rem;
          margin-bottom: 0.5rem;
          flex-wrap: wrap;
        }

        .log-timestamp {
          font-size: 0.75rem;
          color: #6b7280;
          font-family: 'Courier New', monospace;
          min-width: 140px;
        }

        .log-level {
          display: flex;
          align-items: center;
          gap: 0.375rem;
          font-size: 0.75rem;
          font-weight: 600;
          padding: 0.25rem 0.5rem;
          border-radius: 0.25rem;
          min-width: 70px;
        }

        .log-entry.level-debug .log-level {
          background: #f3f4f6;
          color: #6b7280;
        }

        .log-entry.level-info .log-level {
          background: #dbeafe;
          color: #1d4ed8;
        }

        .log-entry.level-warn .log-level {
          background: #fef3c7;
          color: #92400e;
        }

        .log-entry.level-error .log-level {
          background: #fee2e2;
          color: #991b1b;
        }

        .log-category {
          font-size: 0.75rem;
          color: #6b7280;
          background: #e5e7eb;
          padding: 0.25rem 0.5rem;
          border-radius: 0.25rem;
          font-weight: 500;
        }

        .log-message {
          font-size: 0.875rem;
          color: #374151;
          line-height: 1.5;
          margin-bottom: 0.5rem;
        }

        .log-details {
          margin-top: 0.5rem;
        }

        .log-details details {
          background: #f9fafb;
          border: 1px solid #e5e7eb;
          border-radius: 0.25rem;
          overflow: hidden;
        }

        .log-details summary {
          background: #f3f4f6;
          padding: 0.5rem 0.75rem;
          cursor: pointer;
          font-size: 0.75rem;
          font-weight: 500;
          color: #6b7280;
          border-bottom: 1px solid #e5e7eb;
        }

        .log-details summary:hover {
          background: #e5e7eb;
        }

        .log-details pre {
          margin: 0;
          padding: 0.75rem;
          font-size: 0.75rem;
          line-height: 1.4;
          color: #374151;
          background: white;
          overflow-x: auto;
          white-space: pre-wrap;
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

        .btn-error {
          background: #dc2626;
          color: white;
        }

        .btn-error:hover:not(:disabled) {
          background: #b91c1c;
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        /* 스크롤바 스타일링 */
        .logs-list::-webkit-scrollbar {
          width: 8px;
        }

        .logs-list::-webkit-scrollbar-track {
          background: #f1f5f9;
          border-radius: 4px;
        }

        .logs-list::-webkit-scrollbar-thumb {
          background: #cbd5e1;
          border-radius: 4px;
        }

        .logs-list::-webkit-scrollbar-thumb:hover {
          background: #94a3b8;
        }

        /* 애니메이션 */
        @keyframes fadeIn {
          from {
            opacity: 0;
            transform: translateY(10px);
          }
          to {
            opacity: 1;
            transform: translateY(0);
          }
        }

        /* 반응형 디자인 */
        @media (max-width: 768px) {
          .tab-panel {
            padding: 1rem;
          }

          .logs-header {
            flex-direction: column;
            align-items: stretch;
            gap: 1rem;
          }

          .header-left {
            justify-content: space-between;
            flex-wrap: wrap;
          }

          .header-right {
            justify-content: flex-end;
          }

          .logs-controls {
            flex-direction: column;
            align-items: stretch;
          }

          .filters {
            flex-direction: column;
          }

          .search-box {
            max-width: none;
          }

          .auto-refresh {
            justify-content: space-between;
            flex-wrap: wrap;
          }

          .log-header {
            flex-direction: column;
            align-items: flex-start;
            gap: 0.5rem;
          }

          .log-timestamp,
          .log-level,
          .log-category {
            min-width: auto;
          }

          .logs-list {
            padding: 0.75rem;
          }

          .log-entry {
            padding: 0.75rem;
          }
        }

        @media (max-width: 480px) {
          .header-left h3 {
            font-size: 1rem;
          }

          .log-count,
          .last-refresh {
            font-size: 0.625rem;
          }

          .btn-sm {
            padding: 0.25rem 0.5rem;
            font-size: 0.625rem;
          }

          .filters select {
            font-size: 0.75rem;
          }

          .search-box input {
            font-size: 0.75rem;
          }

          .checkbox-label {
            font-size: 0.75rem;
          }
        }

        /* 애니메이션 감소 모드 지원 */
        @media (prefers-reduced-motion: reduce) {
          .log-entry {
            animation: none;
          }

          .btn,
          .log-entry {
            transition: none;
          }
        }
      `}</style>
    </div>
  );
};

export default DeviceLogsTab;