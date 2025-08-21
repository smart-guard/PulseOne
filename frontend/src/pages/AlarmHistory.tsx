// ============================================================================
// frontend/src/pages/AlarmHistory.tsx
// 알람 이력 페이지 - 인라인 스타일 완성본
// ============================================================================

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { AlarmApiService, AlarmOccurrence, AlarmListParams, AlarmStatistics } from '../api/services/alarmApi';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import '../styles/alarm-history.css';
import '../styles/pagination.css';

// 필터 옵션 인터페이스
interface FilterOptions {
  dateRange: {
    start: Date;
    end: Date;
  };
  severity: string;
  state: string;
  searchTerm: string;
}

const AlarmHistory: React.FC = () => {
  // 기본 상태들
  const [alarmEvents, setAlarmEvents] = useState<AlarmOccurrence[]>([]);
  const [statistics, setStatistics] = useState<AlarmStatistics | null>(null);
  const [selectedEvent, setSelectedEvent] = useState<AlarmOccurrence | null>(null);
  
  // 로딩 상태 분리
  const [isInitialLoading, setIsInitialLoading] = useState(true);
  const [isBackgroundRefreshing, setIsBackgroundRefreshing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  
  // UI 상태
  const [viewMode, setViewMode] = useState<'list' | 'timeline'>('list');
  const [showDetailsModal, setShowDetailsModal] = useState(false);
  
  // 필터 상태
  const [filters, setFilters] = useState<FilterOptions>({
    dateRange: {
      start: new Date(Date.now() - 24 * 60 * 60 * 1000),
      end: new Date()
    },
    severity: 'all',
    state: 'all',
    searchTerm: ''
  });
  
  // 페이징 훅 사용
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 10,
    totalCount: 0
  });
  
  // 스크롤 위치 저장용
  const containerRef = useRef<HTMLDivElement>(null);
  const [scrollPosition, setScrollPosition] = useState(0);
  const [hasInitialLoad, setHasInitialLoad] = useState(false);

  // =============================================================================
  // API 호출 함수들
  // =============================================================================
  
  const fetchAlarmHistory = useCallback(async (isBackground = false) => {
    try {
      if (isBackground && containerRef.current) {
        setScrollPosition(containerRef.current.scrollTop);
      }
      
      if (!isBackground) {
        setIsInitialLoading(true);
        setError(null);
      } else {
        setIsBackgroundRefreshing(true);
      }

      const params: AlarmListParams = {
        page: pagination.currentPage,
        limit: pagination.pageSize,
        ...(filters.severity !== 'all' && { severity: filters.severity }),
        ...(filters.state !== 'all' && { state: filters.state }),
        ...(filters.searchTerm && { search: filters.searchTerm }),
        date_from: filters.dateRange.start.toISOString(),
        date_to: filters.dateRange.end.toISOString()
      };

      const response = await AlarmApiService.getAlarmHistory(params);
      
      if (response.success && response.data) {
        const events = response.data.items || [];
        setAlarmEvents(events);
        
        if (response.data.pagination && typeof pagination.setTotalCount === 'function') {
          pagination.setTotalCount(response.data.pagination.total || 0);
        }
        
        if (!hasInitialLoad) {
          setHasInitialLoad(true);
        }
      } else {
        throw new Error(response.message || 'Failed to fetch alarm history');
      }
    } catch (err) {
      console.error('알람 이력 조회 실패:', err);
      setError(err instanceof Error ? err.message : '알람 이력을 불러오는데 실패했습니다');
      
      if (!Array.isArray(alarmEvents)) {
        setAlarmEvents([]);
      }
    } finally {
      setIsInitialLoading(false);
      setIsBackgroundRefreshing(false);
      
      if (isBackground && containerRef.current) {
        setTimeout(() => {
          containerRef.current?.scrollTo(0, scrollPosition);
        }, 100);
      }
    }
  }, [pagination.currentPage, pagination.pageSize, filters, hasInitialLoad, scrollPosition]);

  const fetchStatistics = useCallback(async () => {
    try {
      const response = await AlarmApiService.getAlarmStatistics();
      
      if (response.success && response.data) {
        setStatistics({
          occurrences: {
            total_occurrences: response.data.occurrences?.total_occurrences || 0,
            active_alarms: response.data.occurrences?.active_alarms || 0,
            unacknowledged_alarms: response.data.occurrences?.unacknowledged_alarms || 0,
            acknowledged_alarms: response.data.occurrences?.acknowledged_alarms || 0,
            cleared_alarms: response.data.occurrences?.cleared_alarms || 0
          },
          rules: {
            total_rules: response.data.rules?.total_rules || 0,
            enabled_rules: response.data.rules?.enabled_rules || 0,
            alarm_types: response.data.rules?.alarm_types || 0,
            severity_levels: response.data.rules?.severity_levels || 0,
            target_types: response.data.rules?.target_types || 0,
            categories: response.data.rules?.categories || 0,
            rules_with_tags: response.data.rules?.rules_with_tags || 0
          },
          dashboard_summary: {
            total_active: response.data.dashboard_summary?.total_active || 0,
            total_rules: response.data.dashboard_summary?.total_rules || 0,
            unacknowledged: response.data.dashboard_summary?.unacknowledged || 0,
            enabled_rules: response.data.dashboard_summary?.enabled_rules || 0,
            categories: response.data.dashboard_summary?.categories || 0,
            rules_with_tags: response.data.dashboard_summary?.rules_with_tags || 0
          }
        });
      }
    } catch (err) {
      console.error('알람 통계 조회 실패:', err);
    }
  }, []);

  // =============================================================================
  // 이벤트 핸들러들
  // =============================================================================
  
  const handleRefresh = useCallback(() => {
    fetchAlarmHistory(hasInitialLoad);
    fetchStatistics();
  }, [fetchAlarmHistory, fetchStatistics, hasInitialLoad]);

  const handleFilterChange = useCallback((newFilters: Partial<FilterOptions>) => {
    setFilters(prev => ({ ...prev, ...newFilters }));
    if (typeof pagination.setCurrentPage === 'function') {
      pagination.setCurrentPage(1);
    }
  }, [pagination]);

  const handleSearch = useCallback(() => {
    if (typeof pagination.setCurrentPage === 'function') {
      pagination.setCurrentPage(1);
    }
    fetchAlarmHistory();
  }, [fetchAlarmHistory, pagination]);

  const handleViewDetails = useCallback((event: AlarmOccurrence) => {
    setSelectedEvent(event);
    setShowDetailsModal(true);
  }, []);

  const handleExportToCSV = useCallback(() => {
    if (!Array.isArray(alarmEvents) || alarmEvents.length === 0) {
      console.warn('No alarm events to export');
      return;
    }

    const headers = [
      'ID', '규칙명', '디바이스', '데이터포인트', '심각도', '상태', 
      '발생시간', '확인시간', '해제시간', '확인자', '메모'
    ];
    
    const rows = alarmEvents.map(event => [
      event.id?.toString() || '',
      event.rule_name || '',
      event.device_name || '',
      event.data_point_name || '',
      event.severity || '',
      event.state || '',
      event.occurrence_time ? new Date(event.occurrence_time).toLocaleString('ko-KR') : '',
      event.acknowledged_time ? new Date(event.acknowledged_time).toLocaleString('ko-KR') : '',
      event.cleared_time ? new Date(event.cleared_time).toLocaleString('ko-KR') : '',
      event.acknowledged_by?.toString() || '',
      event.acknowledge_comment || event.clear_comment || ''
    ]);

    const csvContent = [headers, ...rows]
      .map(row => row.map(cell => `"${cell}"`).join(','))
      .join('\n');

    const blob = new Blob(['\uFEFF' + csvContent], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = `alarm_history_${new Date().toISOString().split('T')[0]}.csv`;
    link.click();
  }, [alarmEvents]);

  // =============================================================================
  // 유틸리티 함수들
  // =============================================================================
  
  const formatDuration = (startTime: string, endTime?: string): string => {
    return AlarmApiService.formatDuration(startTime, endTime);
  };

  const getPriorityColor = (severity: string): string => {
    return AlarmApiService.getSeverityColor(severity);
  };

  const getStatusColor = (state: string): string => {
    return AlarmApiService.getStateColor(state);
  };

  const getStatusIcon = (state: string): string => {
    switch (state) {
      case 'active': return 'fas fa-exclamation-circle';
      case 'acknowledged': return 'fas fa-check-circle';
      case 'cleared': return 'fas fa-times-circle';
      default: return 'fas fa-question-circle';
    }
  };

  const getStatusText = (state: string): string => {
    return AlarmApiService.getStateDisplayText(state);
  };

  // =============================================================================
  // 생명주기 및 부수 효과들
  // =============================================================================
  
  useEffect(() => {
    fetchAlarmHistory();
    fetchStatistics();
  }, []);

  useEffect(() => {
    if (hasInitialLoad) {
      fetchAlarmHistory(true);
    }
  }, [pagination.currentPage, pagination.pageSize]);

  useEffect(() => {
    if (hasInitialLoad) {
      fetchAlarmHistory();
    }
  }, [filters]);

  useEffect(() => {
    const interval = setInterval(() => {
      if (hasInitialLoad) {
        handleRefresh();
      }
    }, 30000);

    return () => clearInterval(interval);
  }, [handleRefresh, hasInitialLoad]);

  // =============================================================================
  // 스타일 객체들
  // =============================================================================
  
  const containerStyle = {
    width: '100%',
    background: '#f8fafc',
    minHeight: 'calc(100vh - 64px)',
    padding: '0' // 패딩 제거
  };

  const sectionStyle = {
    margin: '0 24px 24px 24px', // 좌우 동일한 마진
    background: 'white',
    borderRadius: '8px',
    boxShadow: '0 1px 2px 0 rgb(0 0 0 / 0.05)',
    border: '1px solid #e2e8f0'
  };

  const headerStyle = {
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: '24px',
    padding: '24px 24px 16px 24px',
    borderBottom: '1px solid #e2e8f0',
    background: 'white' // 헤더에 배경 추가
  };

  const summaryPanelStyle = {
    ...sectionStyle,
    padding: '24px',
    display: 'grid',
    gridTemplateColumns: 'repeat(5, 1fr)',
    gap: '16px'
  };

  const filterPanelStyle = {
    ...sectionStyle,
    padding: '24px',
    display: 'flex',
    gap: '12px',
    alignItems: 'end'
  };

  const resultInfoStyle = {
    ...sectionStyle,
    padding: '24px',
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center'
  };

  const tableContainerStyle = {
    ...sectionStyle,
    padding: '24px',
    overflow: 'hidden',
    maxHeight: '600px',
    overflowY: 'auto' as const
  };

  const tableStyle = {
    width: '100%',
    borderCollapse: 'collapse' as const,
    fontSize: '16px',
    fontFamily: 'Inter, -apple-system, BlinkMacSystemFont, sans-serif'
  };

  const thStyle = {
    padding: '24px 20px', // 패딩 대폭 증가
    textAlign: 'left' as const,
    fontWeight: 600,
    color: '#334155',
    borderBottom: '2px solid #e2e8f0',
    background: '#f8fafc',
    fontSize: '16px',
    whiteSpace: 'nowrap' as const
  };

  const tdStyle = {
    padding: '24px 20px', // 패딩 대폭 증가 
    borderBottom: '1px solid #f1f5f9',
    verticalAlign: 'top' as const,
    fontSize: '16px',
    lineHeight: 1.6 // 라인 높이 증가
  };

  const rowStyle = {
    transition: 'background-color 0.2s ease',
    minHeight: '60px'
  };

  const btnStyle = {
    display: 'inline-flex',
    alignItems: 'center',
    gap: '8px',
    padding: '8px 16px',
    fontSize: '14px',
    fontWeight: 500,
    borderRadius: '6px',
    border: '1px solid #d1d5db',
    cursor: 'pointer',
    transition: 'all 0.2s ease',
    background: '#f3f4f6',
    color: '#374151'
  };

  // =============================================================================
  // 렌더링
  // =============================================================================
  
  return (
    <div style={containerStyle} ref={containerRef}>
      {/* 페이지 헤더 */}
      <div style={headerStyle}>
        <div>
          <h1 style={{ fontSize: '30px', fontWeight: 700, color: '#1e293b', margin: '0 0 8px 0', display: 'flex', alignItems: 'center', gap: '12px' }}>
            <i className="fas fa-history" style={{ color: '#0ea5e9' }}></i>
            알람 이력
          </h1>
          <p style={{ color: '#64748b', margin: 0, fontSize: '16px', lineHeight: 1.6 }}>
            시스템에서 발생한 모든 알람의 이력을 확인하고 분석할 수 있습니다.
          </p>
        </div>
        <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
          <button
            style={{
              ...btnStyle,
              background: isBackgroundRefreshing ? '#f3f4f6' : '#f3f4f6',
              opacity: isInitialLoading ? 0.5 : 1
            }}
            onClick={handleRefresh}
            disabled={isInitialLoading}
          >
            <i className={`fas fa-sync-alt ${isBackgroundRefreshing ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
          <button
            style={{
              ...btnStyle,
              background: '#0ea5e9',
              color: 'white',
              border: '1px solid #0ea5e9',
              opacity: (!Array.isArray(alarmEvents) || alarmEvents.length === 0) ? 0.5 : 1
            }}
            onClick={handleExportToCSV}
            disabled={!Array.isArray(alarmEvents) || alarmEvents.length === 0}
          >
            <i className="fas fa-download"></i>
            CSV 내보내기
          </button>
        </div>
      </div>

      {/* 통계 요약 패널 */}
      {statistics && (
        <div style={summaryPanelStyle}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', borderLeft: '4px solid #e2e8f0' }}>
            <div style={{ width: '48px', height: '48px', borderRadius: '8px', display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#f1f5f9', color: '#64748b', fontSize: '20px' }}>
              <i className="fas fa-list-alt"></i>
            </div>
            <div>
              <div style={{ fontSize: '24px', fontWeight: 700, color: '#1e293b', lineHeight: 1, marginBottom: '4px' }}>
                {statistics.occurrences.total_occurrences}
              </div>
              <div style={{ fontSize: '14px', color: '#64748b', fontWeight: 500 }}>총 이벤트</div>
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', borderLeft: '4px solid #ef4444' }}>
            <div style={{ width: '48px', height: '48px', borderRadius: '8px', display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#fef2f2', color: '#ef4444', fontSize: '20px' }}>
              <i className="fas fa-exclamation-triangle"></i>
            </div>
            <div>
              <div style={{ fontSize: '24px', fontWeight: 700, color: '#1e293b', lineHeight: 1, marginBottom: '4px' }}>
                {statistics.occurrences.active_alarms}
              </div>
              <div style={{ fontSize: '14px', color: '#64748b', fontWeight: 500 }}>활성 알람</div>
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', borderLeft: '4px solid #f59e0b' }}>
            <div style={{ width: '48px', height: '48px', borderRadius: '8px', display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#fffbeb', color: '#f59e0b', fontSize: '20px' }}>
              <i className="fas fa-check-circle"></i>
            </div>
            <div>
              <div style={{ fontSize: '24px', fontWeight: 700, color: '#1e293b', lineHeight: 1, marginBottom: '4px' }}>
                {statistics.occurrences.acknowledged_alarms}
              </div>
              <div style={{ fontSize: '14px', color: '#64748b', fontWeight: 500 }}>확인된 알람</div>
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', borderLeft: '4px solid #22c55e' }}>
            <div style={{ width: '48px', height: '48px', borderRadius: '8px', display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#f0fdf4', color: '#22c55e', fontSize: '20px' }}>
              <i className="fas fa-times-circle"></i>
            </div>
            <div>
              <div style={{ fontSize: '24px', fontWeight: 700, color: '#1e293b', lineHeight: 1, marginBottom: '4px' }}>
                {statistics.occurrences.cleared_alarms}
              </div>
              <div style={{ fontSize: '14px', color: '#64748b', fontWeight: 500 }}>해제된 알람</div>
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '16px', borderLeft: '4px solid #e2e8f0' }}>
            <div style={{ width: '48px', height: '48px', borderRadius: '8px', display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#f1f5f9', color: '#64748b', fontSize: '20px' }}>
              <i className="fas fa-chart-bar"></i>
            </div>
            <div>
              <div style={{ fontSize: '24px', fontWeight: 700, color: '#1e293b', lineHeight: 1, marginBottom: '4px' }}>
                {statistics.rules.total_rules}
              </div>
              <div style={{ fontSize: '14px', color: '#64748b', fontWeight: 500 }}>총 규칙</div>
            </div>
          </div>
        </div>
      )}

      {/* 에러 메시지 */}
      {error && (
        <div style={{
          ...sectionStyle,
          background: '#fef2f2',
          border: '1px solid #fecaca',
          padding: '16px 24px',
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          color: '#b91c1c'
        }}>
          <i className="fas fa-exclamation-circle" style={{ fontSize: '18px', color: '#ef4444' }}></i>
          <span>{error}</span>
          <button 
            onClick={() => setError(null)}
            style={{ marginLeft: 'auto', background: 'none', border: 'none', color: '#ef4444', cursor: 'pointer', padding: '8px', borderRadius: '6px' }}
          >
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 필터 패널 */}
      <div style={filterPanelStyle}>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', flex: 1.2 }}>
          <label style={{ fontSize: '12px', fontWeight: 600, color: '#334155', marginBottom: '4px' }}>기간</label>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr auto 1fr', gap: '8px', alignItems: 'center' }}>
            <input
              type="datetime-local"
              value={filters.dateRange.start.toISOString().slice(0, 16)}
              onChange={(e) => handleFilterChange({
                dateRange: { ...filters.dateRange, start: new Date(e.target.value) }
              })}
              style={{ padding: '8px 12px', border: '1px solid #d1d5db', borderRadius: '6px', background: 'white', fontSize: '12px', minWidth: 0 }}
            />
            <span style={{ color: '#64748b', fontWeight: 500, fontSize: '12px' }}>~</span>
            <input
              type="datetime-local"
              value={filters.dateRange.end.toISOString().slice(0, 16)}
              onChange={(e) => handleFilterChange({
                dateRange: { ...filters.dateRange, end: new Date(e.target.value) }
              })}
              style={{ padding: '8px 12px', border: '1px solid #d1d5db', borderRadius: '6px', background: 'white', fontSize: '12px', minWidth: 0 }}
            />
          </div>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', flex: 0.15, minWidth: '80px' }}>
          <label style={{ fontSize: '12px', fontWeight: 600, color: '#334155', marginBottom: '4px' }}>심각도</label>
          <select
            value={filters.severity}
            onChange={(e) => handleFilterChange({ severity: e.target.value })}
            style={{ padding: '8px 12px', border: '1px solid #d1d5db', borderRadius: '6px', background: 'white', fontSize: '12px' }}
          >
            <option value="all">전체</option>
            <option value="critical">Critical</option>
            <option value="high">High</option>
            <option value="medium">Medium</option>
            <option value="low">Low</option>
            <option value="info">Info</option>
          </select>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', flex: 0.15, minWidth: '80px' }}>
          <label style={{ fontSize: '12px', fontWeight: 600, color: '#334155', marginBottom: '4px' }}>상태</label>
          <select
            value={filters.state}
            onChange={(e) => handleFilterChange({ state: e.target.value })}
            style={{ padding: '8px 12px', border: '1px solid #d1d5db', borderRadius: '6px', background: 'white', fontSize: '12px' }}
          >
            <option value="all">전체</option>
            <option value="active">활성</option>
            <option value="acknowledged">확인됨</option>
            <option value="cleared">해제됨</option>
          </select>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', flex: 1 }}>
          <label style={{ fontSize: '12px', fontWeight: 600, color: '#334155', marginBottom: '4px' }}>검색</label>
          <div style={{ display: 'flex', gap: '8px', alignItems: 'center', width: '100%' }}>
            <input
              type="text"
              placeholder="알람명, 디바이스명으로 검색..."
              value={filters.searchTerm}
              onChange={(e) => handleFilterChange({ searchTerm: e.target.value })}
              onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
              style={{ flex: 1, padding: '8px 12px', border: '1px solid #d1d5db', borderRadius: '6px', fontSize: '12px' }}
            />
            <button 
              onClick={handleSearch}
              style={{ background: '#0ea5e9', color: 'white', border: 'none', padding: '8px 12px', borderRadius: '6px', fontSize: '12px', fontWeight: 500, cursor: 'pointer' }}
            >
              <i className="fas fa-search"></i>
            </button>
          </div>
        </div>
      </div>

      {/* 결과 정보 */}
      <div style={resultInfoStyle}>
        <div>
          <div style={{ fontSize: '16px', fontWeight: 600, color: '#1e293b' }}>
            총 {pagination.totalCount}개의 알람 이력
            {Array.isArray(alarmEvents) && (
              <span style={{ fontWeight: 400, color: '#64748b' }}> (현재 {alarmEvents.length}개 표시)</span>
            )}
          </div>
        </div>
        
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ fontSize: '14px', color: '#64748b', fontFamily: 'Fira Code, JetBrains Mono, monospace' }}>
            {filters.dateRange.start.toLocaleDateString('ko-KR')} ~ {filters.dateRange.end.toLocaleDateString('ko-KR')}
          </div>
          
          <div style={{ display: 'flex', border: '1px solid #d1d5db', borderRadius: '6px', overflow: 'hidden', background: 'white' }}>
            <button
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                padding: '8px 12px',
                background: viewMode === 'list' ? '#0ea5e9' : 'white',
                color: viewMode === 'list' ? 'white' : '#64748b',
                border: 'none',
                borderRight: '1px solid #d1d5db',
                cursor: 'pointer',
                fontSize: '12px',
                fontWeight: 500,
                whiteSpace: 'nowrap'
              }}
              onClick={() => setViewMode('list')}
            >
              <i className="fas fa-list"></i>
              목록
            </button>
            <button
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                padding: '8px 12px',
                background: viewMode === 'timeline' ? '#0ea5e9' : 'white',
                color: viewMode === 'timeline' ? 'white' : '#64748b',
                border: 'none',
                cursor: 'pointer',
                fontSize: '12px',
                fontWeight: 500,
                whiteSpace: 'nowrap'
              }}
              onClick={() => setViewMode('timeline')}
            >
              <i className="fas fa-chart-line"></i>
              타임라인
            </button>
          </div>
        </div>
      </div>

      {/* 메인 컨텐츠 */}
      {isInitialLoading ? (
        <div style={{ ...sectionStyle, padding: '64px', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', textAlign: 'center' }}>
          <div style={{ width: '48px', height: '48px', border: '4px solid #e2e8f0', borderTop: '4px solid #0ea5e9', borderRadius: '50%', animation: 'spin 1s linear infinite', marginBottom: '16px' }}></div>
          <p style={{ color: '#64748b', fontSize: '16px', margin: 0 }}>알람 이력을 불러오는 중...</p>
        </div>
      ) : !Array.isArray(alarmEvents) || alarmEvents.length === 0 ? (
        <div style={{ ...sectionStyle, padding: '64px', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', textAlign: 'center' }}>
          <div style={{ fontSize: '4rem', color: '#d1d5db', marginBottom: '24px' }}>
            <i className="fas fa-history"></i>
          </div>
          <h3 style={{ fontSize: '20px', fontWeight: 600, color: '#64748b', margin: '0 0 12px 0' }}>알람 이력이 없습니다</h3>
          <p style={{ color: '#9ca3af', margin: 0, lineHeight: 1.6, maxWidth: '400px' }}>
            {!Array.isArray(alarmEvents) 
              ? "데이터 형식 오류가 발생했습니다. 새로고침을 시도해보세요."
              : "검색 조건에 맞는 알람 이력을 찾을 수 없습니다. 필터를 조정해보세요."
            }
          </p>
        </div>
      ) : viewMode === 'list' ? (
        /* 목록 뷰 - 개선된 CSS Grid */
        <div style={tableContainerStyle}>
          {/* 헤더 */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: '60px 100px 1fr 1fr 100px 140px 100px 80px',
            gap: '0',
            background: '#f8fafc',
            borderBottom: '2px solid #e2e8f0',
            position: 'sticky',
            top: 0,
            zIndex: 10,
            minHeight: '50px',
            alignItems: 'center'
          }}>
            <div style={{ padding: '16px 12px', fontWeight: 600, color: '#334155', fontSize: '14px' }}>ID</div>
            <div style={{ padding: '16px 12px', fontWeight: 600, color: '#334155', fontSize: '14px' }}>심각도</div>
            <div style={{ padding: '16px 12px', fontWeight: 600, color: '#334155', fontSize: '14px' }}>디바이스/포인트</div>
            <div style={{ padding: '16px 12px', fontWeight: 600, color: '#334155', fontSize: '14px' }}>메시지</div>
            <div style={{ padding: '16px 12px', fontWeight: 600, color: '#334155', fontSize: '14px' }}>상태</div>
            <div style={{ padding: '16px 12px', fontWeight: 600, color: '#334155', fontSize: '14px' }}>발생시간</div>
            <div style={{ padding: '16px 12px', fontWeight: 600, color: '#334155', fontSize: '14px' }}>지속시간</div>
            <div style={{ padding: '16px 12px', fontWeight: 600, color: '#334155', fontSize: '14px' }}>액션</div>
          </div>

          {/* 데이터 행들 */}
          {alarmEvents.map((event, index) => {
            const eventId = event?.id || index;
            const severity = event?.severity || 'medium';
            const deviceName = event?.device_name || 'N/A';
            const dataPointName = event?.data_point_name || 'N/A';
            const message = event?.alarm_message || '메시지 없음';
            const state = event?.state || 'unknown';
            const occurrenceTime = event?.occurrence_time || new Date().toISOString();
            const triggeredValue = event?.trigger_value;
            
            return (
              <div
                key={eventId}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '60px 100px 1fr 1fr 100px 140px 100px 80px',
                  gap: '0',
                  borderLeft: `4px solid ${getPriorityColor(severity)}`,
                  borderBottom: '1px solid #f1f5f9',
                  opacity: isBackgroundRefreshing ? 0.7 : 1,
                  minHeight: '60px',
                  alignItems: 'center',
                  background: 'white'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.background = '#f8fafc';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.background = 'white';
                }}
              >
                {/* ID */}
                <div style={{ padding: '12px 8px' }}>
                  <span style={{ fontFamily: 'monospace', fontWeight: 600, color: '#64748b', fontSize: '12px' }}>
                    #{eventId}
                  </span>
                </div>
                
                {/* 심각도 */}
                <div style={{ padding: '12px 8px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                    <div 
                      style={{ 
                        width: '10px', 
                        height: '10px', 
                        borderRadius: '50%', 
                        background: getPriorityColor(severity),
                        flexShrink: 0
                      }}
                    ></div>
                    <span style={{ fontSize: '12px', fontWeight: 600 }}>{severity.toUpperCase()}</span>
                  </div>
                </div>
                
                {/* 디바이스/포인트 */}
                <div style={{ padding: '12px 8px' }}>
                  <div>
                    <div style={{ fontWeight: 600, color: '#1e293b', fontSize: '14px', marginBottom: '2px' }}>
                      {deviceName}
                    </div>
                    <div style={{ fontSize: '12px', color: '#64748b' }}>
                      {dataPointName}
                    </div>
                  </div>
                </div>
                
                {/* 메시지 */}
                <div style={{ padding: '12px 8px' }}>
                  <div>
                    <div style={{ color: '#334155', fontSize: '14px', lineHeight: 1.4 }}>
                      {message}
                    </div>
                    {triggeredValue !== null && triggeredValue !== undefined && (
                      <div style={{
                        fontSize: '11px',
                        color: '#0284c7',
                        fontFamily: 'monospace',
                        background: '#f0f9ff',
                        padding: '2px 6px',
                        borderRadius: '3px',
                        display: 'inline-block',
                        marginTop: '2px'
                      }}>
                        값: {triggeredValue}
                      </div>
                    )}
                  </div>
                </div>
                
                {/* 상태 */}
                <div style={{ padding: '12px 8px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                    <div style={{ color: getStatusColor(state), fontSize: '14px' }}>
                      <i className={getStatusIcon(state)}></i>
                    </div>
                    <span style={{ fontSize: '12px', fontWeight: 500 }}>{getStatusText(state)}</span>
                  </div>
                </div>
                
                {/* 발생시간 */}
                <div style={{ padding: '12px 8px' }}>
                  <div style={{ fontFamily: 'monospace' }}>
                    <div style={{ color: '#1e293b', fontSize: '12px', marginBottom: '2px' }}>
                      {new Date(occurrenceTime).toLocaleDateString('ko-KR')}
                    </div>
                    <div style={{ fontSize: '11px', color: '#64748b' }}>
                      {new Date(occurrenceTime).toLocaleTimeString('ko-KR')}
                    </div>
                  </div>
                </div>
                
                {/* 지속시간 */}
                <div style={{ padding: '12px 8px' }}>
                  <div style={{ fontFamily: 'monospace', color: '#64748b', fontSize: '12px' }}>
                    {formatDuration(occurrenceTime, event?.cleared_time || event?.acknowledged_time)}
                  </div>
                </div>
                
                {/* 액션 */}
                <div style={{ padding: '12px 8px', textAlign: 'center' }}>
                  <button
                    style={{
                      padding: '4px 8px',
                      fontSize: '11px',
                      fontWeight: 500,
                      borderRadius: '4px',
                      border: '1px solid #d1d5db',
                      background: '#f3f4f6',
                      color: '#374151',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px'
                    }}
                    onClick={() => handleViewDetails(event)}
                  >
                    <i className="fas fa-eye" style={{ fontSize: '10px' }}></i>
                    상세
                  </button>
                </div>
              </div>
            );
          })}
        </div>
      ) : (
        /* 타임라인 뷰 */
        <div style={{ ...sectionStyle, padding: '24px', maxHeight: '600px', overflowY: 'auto' }}>
          <div style={{ position: 'relative' }}>
            {alarmEvents.map((event, index) => {
              const eventId = event?.id || index;
              const severity = event?.severity || 'medium';
              const ruleName = event?.rule_name || '알 수 없는 규칙';
              const deviceName = event?.device_name || 'N/A';
              const dataPointName = event?.data_point_name || 'N/A';
              const message = event?.alarm_message || '메시지 없음';
              const state = event?.state || 'unknown';
              const occurrenceTime = event?.occurrence_time || new Date().toISOString();
              
              return (
                <div key={eventId} style={{ display: 'flex', gap: '20px', marginBottom: '32px', position: 'relative' }}>
                  <div style={{ flexShrink: 0, position: 'relative' }}>
                    <div 
                      style={{
                        width: '32px',
                        height: '32px',
                        borderRadius: '50%',
                        border: '4px solid white',
                        boxShadow: `0 0 0 2px #e2e8f0`,
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        position: 'relative',
                        zIndex: 2,
                        background: getPriorityColor(severity)
                      }}
                    ></div>
                    {index < alarmEvents.length - 1 && (
                      <div style={{
                        position: 'absolute',
                        left: '15px',
                        top: '60px',
                        bottom: '-32px',
                        width: '2px',
                        background: '#e2e8f0'
                      }}></div>
                    )}
                  </div>
                  <div style={{
                    flex: 1,
                    background: '#f8fafc',
                    borderRadius: '8px',
                    padding: '20px',
                    border: '1px solid #e2e8f0',
                    transition: 'all 0.2s ease'
                  }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '16px', gap: '16px' }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '12px', flex: 1 }}>
                        <span 
                          style={{
                            padding: '4px 12px',
                            borderRadius: '12px',
                            color: 'white',
                            fontSize: '12px',
                            fontWeight: 600,
                            textTransform: 'uppercase',
                            background: getPriorityColor(severity)
                          }}
                        >
                          {severity.toUpperCase()}
                        </span>
                        <span style={{ fontSize: '16px', fontWeight: 500 }}>{ruleName}</span>
                      </div>
                      <div style={{ fontSize: '14px', color: '#64748b', fontFamily: 'Fira Code, JetBrains Mono, monospace', whiteSpace: 'nowrap' }}>
                        {new Date(occurrenceTime).toLocaleString('ko-KR')}
                      </div>
                    </div>
                    <div>
                      <div style={{ fontWeight: 500, color: '#1e293b', marginBottom: '12px', lineHeight: 1.5, fontSize: '16px' }}>
                        {message}
                      </div>
                      <div style={{ fontSize: '14px', color: '#64748b', marginBottom: '16px' }}>
                        {deviceName} • {dataPointName}
                      </div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', fontSize: '14px', paddingTop: '12px', borderTop: '1px solid #e2e8f0' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontWeight: 500 }}>
                          <i 
                            className={getStatusIcon(state)} 
                            style={{ color: getStatusColor(state) }}
                          ></i>
                          {getStatusText(state)}
                        </div>
                        <div style={{ color: '#64748b', fontFamily: 'Fira Code, JetBrains Mono, monospace' }}>
                          지속시간: {formatDuration(occurrenceTime, event?.cleared_time || event?.acknowledged_time)}
                        </div>
                      </div>
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      )}

      {/* 페이지네이션 */}
      {Array.isArray(alarmEvents) && pagination.totalPages > 1 && (
        <Pagination
          currentPage={pagination.currentPage}
          totalPages={pagination.totalPages}
          pageSize={pagination.pageSize}
          totalCount={pagination.totalCount}
          onPageChange={pagination.setCurrentPage}
          onPageSizeChange={pagination.setPageSize}
          disabled={isInitialLoading}
        />
      )}

      {/* 상세 보기 모달 */}
      {showDetailsModal && selectedEvent && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: 'rgba(0, 0, 0, 0.5)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 1000,
          padding: '24px',
          backdropFilter: 'blur(4px)'
        }}>
          <div style={{
            background: 'white',
            borderRadius: '8px',
            boxShadow: '0 20px 25px -5px rgb(0 0 0 / 0.1), 0 8px 10px -6px rgb(0 0 0 / 0.1)',
            width: '100%',
            maxWidth: '800px',
            maxHeight: '90vh',
            display: 'flex',
            flexDirection: 'column'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              padding: '24px',
              borderBottom: '1px solid #e2e8f0'
            }}>
              <h3 style={{ margin: 0, fontSize: '20px', fontWeight: 600, color: '#1e293b' }}>알람 상세 정보</h3>
              <button 
                onClick={() => setShowDetailsModal(false)}
                style={{
                  background: 'none',
                  border: 'none',
                  fontSize: '20px',
                  color: '#64748b',
                  cursor: 'pointer',
                  padding: '8px',
                  borderRadius: '6px',
                  transition: 'all 0.2s ease'
                }}
              >
                <i className="fas fa-times"></i>
              </button>
            </div>
            <div style={{ flex: 1, overflowY: 'auto', padding: '24px' }}>
              <div style={{ marginBottom: '32px' }}>
                <h4 style={{ margin: '0 0 16px 0', fontSize: '18px', fontWeight: 600, color: '#334155', borderBottom: '1px solid #e2e8f0', paddingBottom: '8px' }}>
                  기본 정보
                </h4>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(250px, 1fr))', gap: '20px' }}>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                      이벤트 ID
                    </label>
                    <span style={{ fontSize: '14px', color: '#334155', lineHeight: 1.5 }}>#{selectedEvent.id}</span>
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                      알람 규칙
                    </label>
                    <span style={{ fontSize: '14px', color: '#334155', lineHeight: 1.5 }}>{selectedEvent.rule_name}</span>
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                      심각도
                    </label>
                    <span 
                      style={{
                        display: 'inline-block',
                        padding: '4px 12px',
                        borderRadius: '12px',
                        color: 'white',
                        fontSize: '12px',
                        fontWeight: 600,
                        textTransform: 'uppercase',
                        background: getPriorityColor(selectedEvent.severity)
                      }}
                    >
                      {selectedEvent.severity.toUpperCase()}
                    </span>
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                      상태
                    </label>
                    <span style={{ fontSize: '14px', color: getStatusColor(selectedEvent.state), lineHeight: 1.5 }}>
                      <i className={getStatusIcon(selectedEvent.state)}></i>
                      {' ' + getStatusText(selectedEvent.state)}
                    </span>
                  </div>
                </div>
              </div>

              <div style={{ marginBottom: '32px' }}>
                <h4 style={{ margin: '0 0 16px 0', fontSize: '18px', fontWeight: 600, color: '#334155', borderBottom: '1px solid #e2e8f0', paddingBottom: '8px' }}>
                  시간 정보
                </h4>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(250px, 1fr))', gap: '20px' }}>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                      발생시간
                    </label>
                    <span style={{ fontSize: '14px', color: '#334155', lineHeight: 1.5 }}>
                      {new Date(selectedEvent.occurrence_time).toLocaleString('ko-KR')}
                    </span>
                  </div>
                  {selectedEvent.acknowledged_time && (
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                      <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                        확인시간
                      </label>
                      <span style={{ fontSize: '14px', color: '#334155', lineHeight: 1.5 }}>
                        {new Date(selectedEvent.acknowledged_time).toLocaleString('ko-KR')}
                      </span>
                    </div>
                  )}
                  {selectedEvent.cleared_time && (
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                      <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                        해제시간
                      </label>
                      <span style={{ fontSize: '14px', color: '#334155', lineHeight: 1.5 }}>
                        {new Date(selectedEvent.cleared_time).toLocaleString('ko-KR')}
                      </span>
                    </div>
                  )}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                      지속시간
                    </label>
                    <span style={{ fontSize: '14px', color: '#334155', lineHeight: 1.5 }}>
                      {formatDuration(selectedEvent.occurrence_time, selectedEvent.cleared_time || selectedEvent.acknowledged_time)}
                    </span>
                  </div>
                </div>
              </div>

              <div style={{ marginBottom: '32px' }}>
                <h4 style={{ margin: '0 0 16px 0', fontSize: '18px', fontWeight: 600, color: '#334155', borderBottom: '1px solid #e2e8f0', paddingBottom: '8px' }}>
                  메시지 및 설명
                </h4>
                <div style={{ background: '#f8fafc', borderRadius: '8px', padding: '20px', border: '1px solid #e2e8f0' }}>
                  <p style={{ margin: '0 0 12px 0', lineHeight: 1.6 }}>
                    <strong>메시지:</strong> {selectedEvent.alarm_message}
                  </p>
                  {selectedEvent.trigger_value !== null && selectedEvent.trigger_value !== undefined && (
                    <p style={{ margin: '0', lineHeight: 1.6 }}>
                      <strong>발생값:</strong> {selectedEvent.trigger_value}
                    </p>
                  )}
                </div>
              </div>

              {(selectedEvent.acknowledged_by || selectedEvent.acknowledge_comment) && (
                <div style={{ marginBottom: '32px' }}>
                  <h4 style={{ margin: '0 0 16px 0', fontSize: '18px', fontWeight: 600, color: '#334155', borderBottom: '1px solid #e2e8f0', paddingBottom: '8px' }}>
                    확인 정보
                  </h4>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(250px, 1fr))', gap: '20px' }}>
                    {selectedEvent.acknowledged_by && (
                      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                        <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                          확인자
                        </label>
                        <span style={{ fontSize: '14px', color: '#334155', lineHeight: 1.5 }}>{selectedEvent.acknowledged_by}</span>
                      </div>
                    )}
                    {selectedEvent.acknowledge_comment && (
                      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', gridColumn: '1 / -1' }}>
                        <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                          확인 메모
                        </label>
                        <span style={{ fontSize: '14px', color: '#334155', lineHeight: 1.5 }}>{selectedEvent.acknowledge_comment}</span>
                      </div>
                    )}
                  </div>
                </div>
              )}

              {selectedEvent.clear_comment && (
                <div style={{ marginBottom: '32px' }}>
                  <h4 style={{ margin: '0 0 16px 0', fontSize: '18px', fontWeight: 600, color: '#334155', borderBottom: '1px solid #e2e8f0', paddingBottom: '8px' }}>
                    해제 정보
                  </h4>
                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(250px, 1fr))', gap: '20px' }}>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', gridColumn: '1 / -1' }}>
                      <label style={{ fontSize: '12px', fontWeight: 600, color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.05em' }}>
                        해제 메모
                      </label>
                      <span style={{ fontSize: '14px', color: '#334155', lineHeight: 1.5 }}>{selectedEvent.clear_comment}</span>
                    </div>
                  </div>
                </div>
              )}
            </div>
            <div style={{
              display: 'flex',
              justifyContent: 'flex-end',
              gap: '12px',
              padding: '24px',
              borderTop: '1px solid #e2e8f0'
            }}>
              <button 
                style={{
                  ...btnStyle,
                  background: '#f3f4f6',
                  color: '#374151'
                }}
                onClick={() => setShowDetailsModal(false)}
              >
                닫기
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default AlarmHistory;