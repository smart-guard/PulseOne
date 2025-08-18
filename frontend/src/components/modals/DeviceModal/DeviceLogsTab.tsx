// ============================================================================
// frontend/src/components/modals/DeviceModal/DeviceLogsTab.tsx
// 📄 디바이스 로그 탭 컴포넌트 - 강화된 스크롤 및 더미데이터
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
  const [autoRefresh, setAutoRefresh] = useState(false); // 🔥 기본값 false로 변경
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
      'Diagnostic mode enabled',
      'TCP connection timeout after 30 seconds',
      'Modbus register 40001 read successfully',
      'Device response time: 250ms',
      'Checksum validation failed',
      'Retry attempt 3/5 for device communication',
      'Device firmware version: v2.1.3',
      'Buffer overflow detected in data parser',
      'Network interface eth0 status changed',
      'Device driver initialized successfully',
      'Watchdog timer reset',
      'Certificate validation completed',
      'SSL handshake failed - certificate expired',
      'Data encryption enabled for secure transmission',
      'Device ID mismatch detected',
      'Protocol version compatibility check passed',
      'Emergency stop signal received from device'
    ];

    return Array.from({ length: 150 }, (_, i) => {
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
          errorCode: Math.floor(Math.random() * 1000),
          stackTrace: 'at ModbusClient.read() line 127',
          errorData: { register: 40001, value: null }
        } : undefined
      };
    });
  };

  // ========================================================================
  // 이벤트 핸들러
  // ========================================================================
  
  const loadLogs = async () => {
    setIsLoading(true);
    setError(null);
    
    try {
      // Mock API 호출 시뮬레이션
      await new Promise(resolve => setTimeout(resolve, 500)); // 🔥 로딩 시간 단축
      
      // 🔥 기존 로그가 있으면 유지하고 새 로그만 추가
      if (logs.length === 0) {
        const newLogs = generateMockLogs();
        setLogs(newLogs);
      } else {
        // 실제로는 새 로그만 추가하는 로직
        // 여기서는 데모를 위해 기존 로그 유지
      }
      
      setLastRefresh(new Date());
    } catch (err) {
      setError('로그를 불러오는데 실패했습니다.');
    } finally {
      setIsLoading(false);
    }
  };

  const exportLogs = () => {
    const filteredLogs = getFilteredLogs();
    const csvContent = [
      'Timestamp,Level,Category,Message',
      ...filteredLogs.map(log => 
        `"${log.timestamp}","${log.level}","${log.category}","${log.message.replace(/"/g, '""')}"`
      )
    ].join('\n');
    
    const blob = new Blob([csvContent], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `device-${deviceId}-logs-${new Date().toISOString().split('T')[0]}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const clearLogs = () => {
    if (confirm('모든 로그를 삭제하시겠습니까?')) {
      setLogs([]);
    }
  };

  const scrollToBottom = () => {
    logsEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  // ========================================================================
  // 필터링 로직
  // ========================================================================
  
  const getFilteredLogs = () => {
    return logs.filter(log => {
      const levelMatch = filterLevel === 'ALL' || log.level === filterLevel;
      const categoryMatch = filterCategory === 'ALL' || log.category === filterCategory;
      const searchMatch = !searchTerm || 
        log.message.toLowerCase().includes(searchTerm.toLowerCase()) ||
        log.category.toLowerCase().includes(searchTerm.toLowerCase());
      
      return levelMatch && categoryMatch && searchMatch;
    });
  };

  // ========================================================================
  // 라이프사이클
  // ========================================================================
  
  useEffect(() => {
    loadLogs();
  }, [deviceId]);

  useEffect(() => {
    if (autoRefresh && refreshInterval > 0) {
      intervalRef.current = setInterval(() => {
        if (!isLoading) {
          // 🔥 새로고침 시 전체 재로딩 대신 타임스탬프만 업데이트
          setLastRefresh(new Date());
          // 실제로는 새 로그만 가져오는 API 호출
        }
      }, refreshInterval * 1000);
    }
    
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, [autoRefresh, refreshInterval, isLoading]);

  useEffect(() => {
    if (isRealTime) {
      scrollToBottom();
    }
  }, [logs, isRealTime]);

  // ========================================================================
  // 헬퍼 함수
  // ========================================================================
  
  const getLevelIcon = (level: string) => {
    switch (level) {
      case 'DEBUG': return 'fa-bug';
      case 'INFO': return 'fa-info-circle';
      case 'WARN': return 'fa-exclamation-triangle';
      case 'ERROR': return 'fa-times-circle';
      default: return 'fa-circle';
    }
  };

  const formatTimestamp = (timestamp: string) => {
    return new Date(timestamp).toLocaleString('ko-KR', {
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  };

  // ========================================================================
  // 로그 엔트리 컴포넌트
  // ========================================================================
  
  const LogEntry: React.FC<{ log: DeviceLogEntry }> = ({ log }) => (
    <div className={`log-entry level-${log.level.toLowerCase()}`}>
      <div className="log-header">
        <div className="log-timestamp">{formatTimestamp(log.timestamp)}</div>
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
    <div className="logs-tab-wrapper">
      <div className="logs-tab-container">
        
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
            
            <select value={filterLevel} onChange={(e) => setFilterLevel(e.target.value)}>
              <option value="ALL">모든 레벨</option>
              <option value="DEBUG">DEBUG</option>
              <option value="INFO">INFO</option>
              <option value="WARN">WARN</option>
              <option value="ERROR">ERROR</option>
            </select>
            
            <select value={filterCategory} onChange={(e) => setFilterCategory(e.target.value)}>
              <option value="ALL">모든 카테고리</option>
              <option value="COMMUNICATION">통신</option>
              <option value="DATA_COLLECTION">데이터수집</option>
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
                onChange={(e) => setRefreshInterval(Number(e.target.value))}
              >
                <option value={3}>3초</option>
                <option value={5}>5초</option>
                <option value={10}>10초</option>
                <option value={30}>30초</option>
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
            
            <button className="btn btn-secondary btn-sm" onClick={scrollToBottom}>
              <i className="fas fa-arrow-down"></i>
              맨 아래로
            </button>
          </div>
        </div>

        {/* 통계 정보 */}
        <div className="logs-stats">
          <div className="stats-grid">
            <div className="stat-item debug">
              <div className="stat-label">DEBUG</div>
              <div className="stat-value">{logs.filter(l => l.level === 'DEBUG').length}</div>
            </div>
            <div className="stat-item info">
              <div className="stat-label">INFO</div>
              <div className="stat-value">{logs.filter(l => l.level === 'INFO').length}</div>
            </div>
            <div className="stat-item warn">
              <div className="stat-label">WARN</div>
              <div className="stat-value">{logs.filter(l => l.level === 'WARN').length}</div>
            </div>
            <div className="stat-item error">
              <div className="stat-label">ERROR</div>
              <div className="stat-value">{logs.filter(l => l.level === 'ERROR').length}</div>
            </div>
          </div>
        </div>

        {/* 에러 메시지 */}
        {error && (
          <div className="error-message">⚠️ {error}</div>
        )}

        {/* 🔥 로그 목록 */}
        <div className="logs-list-container">
          <div className="logs-list">
            {isLoading ? (
              <div className="empty-state">
                <i className="fas fa-spinner fa-spin fa-2x"></i>
                <p>로그를 불러오는 중...</p>
              </div>
            ) : filteredLogs.length === 0 ? (
              <div className="empty-state">
                <i className="fas fa-file-alt fa-2x"></i>
                <p>{logs.length === 0 ? '로그가 없습니다.' : '검색 결과가 없습니다.'}</p>
              </div>
            ) : (
              <>
                {filteredLogs.map((log) => (
                  <LogEntry key={log.id} log={log} />
                ))}
                <div ref={logsEndRef} />
              </>
            )}
          </div>
        </div>

        {/* 로그 정보 */}
        <div className="logs-info">
          <div className="info-sections">
            <div className="info-section">
              <h4>📊 카테고리별 분포</h4>
              <div className="category-distribution">
                {['COMMUNICATION', 'DATA_COLLECTION', 'CONNECTION', 'SYSTEM', 'PROTOCOL'].map(cat => (
                  <div key={cat} className="category-item">
                    <span className="category-label">{cat}</span>
                    <span className="category-count">{logs.filter(l => l.category === cat).length}개</span>
                  </div>
                ))}
              </div>
            </div>

            <div className="info-section">
              <h4>⚙️ 로그 설정</h4>
              <div className="log-settings">
                <button className="btn btn-outline">로그 레벨 변경</button>
                <button className="btn btn-outline">필터 저장</button>
                <button className="btn btn-outline">설정 초기화</button>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* 스타일 */}
      <style jsx>{`
        /* 🔥 최상위 래퍼 - 전체 스크롤 활성화 */
        .logs-tab-wrapper {
          height: 100%;
          width: 100%;
          overflow-y: auto;
          background: #f8fafc;
          padding: 1rem;
        }

        .logs-tab-container {
          display: flex;
          flex-direction: column;
          gap: 1rem;
          min-height: calc(100vh - 200px);
        }

        /* 🔥 헤더 */
        .logs-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          background: white;
          padding: 1rem;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          flex-shrink: 0;
        }

        .header-left {
          display: flex;
          align-items: center;
          gap: 1rem;
          flex-wrap: wrap;
        }

        .header-left h3 {
          margin: 0;
          font-size: 1.125rem;
          font-weight: 600;
        }

        .log-count {
          background: #0ea5e9;
          color: white;
          padding: 0.25rem 0.75rem;
          border-radius: 9999px;
          font-size: 0.75rem;
        }

        .last-refresh {
          font-size: 0.75rem;
          color: #6b7280;
        }

        .header-right {
          display: flex;
          gap: 0.75rem;
        }

        /* 🔥 컨트롤 */
        .logs-controls {
          display: flex;
          flex-direction: column;
          gap: 1rem;
          background: white;
          padding: 1rem;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          flex-shrink: 0;
        }

        .filters {
          display: flex;
          gap: 1rem;
          align-items: center;
          flex-wrap: wrap;
        }

        .search-box {
          position: relative;
          flex: 1;
          min-width: 200px;
        }

        .search-box input {
          width: 100%;
          padding: 0.5rem 2rem 0.5rem 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 6px;
        }

        .search-box i {
          position: absolute;
          right: 0.75rem;
          top: 50%;
          transform: translateY(-50%);
          color: #6b7280;
        }

        .filters select {
          padding: 0.5rem;
          border: 1px solid #d1d5db;
          border-radius: 6px;
          background: white;
        }

        .auto-refresh {
          display: flex;
          align-items: center;
          gap: 1rem;
          flex-wrap: wrap;
        }

        .checkbox-label {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          cursor: pointer;
          font-size: 0.875rem;
        }

        .checkbox-label input {
          margin: 0;
        }

        /* 🔥 통계 정보 */
        .logs-stats {
          background: white;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          padding: 1.5rem;
          flex-shrink: 0;
        }

        .stats-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
          gap: 1.5rem;
        }

        .stat-item {
          text-align: center;
          padding: 1rem;
          border-radius: 8px;
        }

        .stat-item.debug {
          background: #f3f4f6;
        }

        .stat-item.info {
          background: #dbeafe;
        }

        .stat-item.warn {
          background: #fef3c7;
        }

        .stat-item.error {
          background: #fee2e2;
        }

        .stat-label {
          font-size: 0.75rem;
          color: #6b7280;
          margin-bottom: 0.5rem;
          text-transform: uppercase;
          letter-spacing: 0.05em;
        }

        .stat-value {
          font-size: 1.5rem;
          font-weight: 600;
        }

        .stat-item.debug .stat-value {
          color: #6b7280;
        }

        .stat-item.info .stat-value {
          color: #1d4ed8;
        }

        .stat-item.warn .stat-value {
          color: #92400e;
        }

        .stat-item.error .stat-value {
          color: #991b1b;
        }

        /* 🔥 에러 메시지 */
        .error-message {
          background: #fef2f2;
          color: #dc2626;
          padding: 1rem;
          border-radius: 8px;
          border: 1px solid #fecaca;
          flex-shrink: 0;
        }

        /* 🔥 로그 목록 컨테이너 - 강제 스크롤 */
        .logs-list-container {
          background: white;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          overflow: hidden;
          display: flex;
          flex-direction: column;
          height: 500px; /* 🔥 고정 높이로 강제 스크롤 */
        }

        /* 🔥 로그 목록 - 스크롤 영역 */
        .logs-list {
          flex: 1;
          overflow-y: scroll !important;
          overflow-x: hidden !important;
          padding: 1rem;
          scrollbar-width: thin;
          scrollbar-color: #94a3b8 #f1f5f9;
        }

        /* 🔥 강화된 스크롤바 스타일 */
        .logs-list::-webkit-scrollbar {
          width: 14px !important;
          background: #f1f5f9 !important;
          border-left: 1px solid #e5e7eb !important;
        }

        .logs-list::-webkit-scrollbar-track {
          background: #f8fafc !important;
          border-radius: 0 !important;
        }

        .logs-list::-webkit-scrollbar-thumb {
          background: #94a3b8 !important;
          border-radius: 7px !important;
          border: 2px solid #f8fafc !important;
          min-height: 40px !important;
        }

        .logs-list::-webkit-scrollbar-thumb:hover {
          background: #64748b !important;
        }

        .logs-list::-webkit-scrollbar-thumb:active {
          background: #475569 !important;
        }

        /* 🔥 Firefox 스크롤바 */
        .logs-list {
          scrollbar-width: auto !important;
          scrollbar-color: #94a3b8 #f8fafc !important;
        }

        /* 🔥 로그 엔트리 - 깜빡임 방지 */
        .log-entry {
          background: white;
          border: 1px solid #e5e7eb;
          border-radius: 8px;
          padding: 1rem;
          margin-bottom: 0.75rem;
          transition: box-shadow 0.2s ease, border-color 0.2s ease; /* 🔥 애니메이션 최소화 */
        }

        .log-entry:hover {
          box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
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

        /* 🔥 빈 상태 */
        .empty-state {
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          padding: 4rem 2rem;
          color: #6b7280;
          text-align: center;
        }

        .empty-state i {
          margin-bottom: 1rem;
          color: #d1d5db;
        }

        .empty-state p {
          margin: 0;
          font-size: 1rem;
        }

        /* 🔥 추가 정보 섹션 */
        .logs-info {
          background: white;
          border-radius: 8px;
          border: 1px solid #e5e7eb;
          padding: 1.5rem;
          flex-shrink: 0;
        }

        .info-sections {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
          gap: 2rem;
        }

        .info-section h4 {
          margin: 0 0 1rem 0;
          font-size: 1rem;
          font-weight: 600;
          color: #374151;
        }

        .category-distribution {
          display: flex;
          flex-direction: column;
          gap: 0.75rem;
        }

        .category-item {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 0.5rem;
          background: #f8fafc;
          border-radius: 6px;
        }

        .category-label {
          font-size: 0.875rem;
          color: #374151;
        }

        .category-count {
          font-size: 0.875rem;
          font-weight: 600;
          color: #0ea5e9;
        }

        .log-settings {
          display: flex;
          flex-direction: column;
          gap: 0.75rem;
        }

        /* 🔥 버튼 스타일 */
        .btn {
          display: inline-flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border: none;
          border-radius: 6px;
          font-size: 0.875rem;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s;
          white-space: nowrap;
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

        .btn-outline {
          background: transparent;
          color: #0ea5e9;
          border: 1px solid #0ea5e9;
        }

        .btn-outline:hover {
          background: #0ea5e9;
          color: white;
        }

        .btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        /* 🔥 애니메이션 */
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

        /* 🔥 반응형 */
        @media (max-width: 768px) {
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

          .info-sections {
            grid-template-columns: 1fr;
          }
        }
      `}</style>
    </div>
  );
};

export default DeviceLogsTab;