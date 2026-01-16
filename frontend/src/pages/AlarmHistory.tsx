// ============================================================================
// frontend/src/pages/AlarmHistory.tsx
// 알람 이력 페이지 - 하이브리드 접근 (중요 레이아웃은 인라인, 반복 스타일은 CSS)
// ============================================================================

import React, { useState, useEffect, useCallback, useRef, useMemo } from 'react';
import { AlarmApiService, AlarmOccurrence, AlarmListParams, AlarmStatistics } from '../api/services/alarmApi';
import { Pagination } from '../components/common/Pagination';
import { ManagementLayout } from '../components/common/ManagementLayout';
import { PageHeader } from '../components/common/PageHeader';
import { StatCard } from '../components/common/StatCard';
import { useConfirmContext } from '../components/common/ConfirmProvider';
import { useAlarmContext } from '../contexts/AlarmContext';
import { usePagination } from '../hooks/usePagination';
import '../styles/management.css';
import '../styles/alarm-history.css';

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
  const { confirm } = useConfirmContext();
  const { decrementAlarmCount } = useAlarmContext();
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
  const [activeRowId, setActiveRowId] = useState<number | null>(null);

  // 필터 상태
  const [filters, setFilters] = useState<FilterOptions>({
    dateRange: {
      start: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000), // 7일 전으로 변경
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
  // 원본 인라인 스타일 객체들 (중요한 레이아웃 유지)
  // =============================================================================

  const containerStyle = {
    width: '100%',
    background: '#f8fafc',
    minHeight: 'calc(100vh - 64px)',
    padding: '0'
  };

  const sectionStyle = {
    margin: '0 24px 24px 24px',
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
    padding: '24px 0 16px 0', // 패딩 제거하고 마진과 정렬
    margin: '0 24px', // 섹션들과 동일한 마진 적용
    borderBottom: '1px solid #e2e8f0',
    background: '#f8fafc' // 배경색을 컨테이너와 동일하게 변경하여 경계 제거
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
    padding: '0',
    overflow: 'hidden',
    height: '600px',
    display: 'flex',
    flexDirection: 'column' as const
  };

  const tableWrapperStyle = {
    flex: 1,
    overflowY: 'auto' as const,
    overflowX: 'auto' as const,
    scrollbarWidth: 'thin' as const,
    msOverflowStyle: 'scrollbar' as const
  };

  const gridContainerStyle = {
    display: 'grid',
    gridTemplateColumns: '0.6fr 0.8fr 1fr 1.8fr 0.6fr 0.8fr 0.6fr 0.8fr',
    gap: '0',
    minWidth: '1000px',
    fontSize: '14px'
  };

  const gridItemStyle = {
    padding: '16px 12px',
    borderBottom: '1px solid #f1f5f9',
    display: 'flex',
    alignItems: 'center',
    minHeight: '56px'
  };

  const thStyle = {
    padding: '24px 20px',
    textAlign: 'left' as const,
    fontWeight: 600,
    color: '#334155',
    borderBottom: '2px solid #e2e8f0',
    background: '#f8fafc',
    fontSize: '16px',
    whiteSpace: 'nowrap' as const
  };

  // =============================================================================
  // API 호출 및 이벤트 핸들러들 (기존 코드 유지)
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

        if (response.data.pagination && typeof pagination.updateTotalCount === 'function') {
          const totalFromServer = response.data.pagination.total || 0;
          // 서버에서 온 total이 0이더라도 실제 아이템이 있으면 아이템 개수로 대체
          pagination.updateTotalCount(totalFromServer > 0 ? totalFromServer : (events.length || 0));
        } else if (events.length > 0) {
          pagination.updateTotalCount(events.length);
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

  const handleRefresh = useCallback(() => {
    fetchAlarmHistory(hasInitialLoad);
    fetchStatistics();
  }, [fetchAlarmHistory, fetchStatistics, hasInitialLoad]);

  const handleFilterChange = useCallback((newFilters: Partial<FilterOptions>) => {
    setFilters(prev => ({ ...prev, ...newFilters }));
    if (typeof pagination.goToPage === 'function') {
      pagination.goToPage(1);
    }
  }, [pagination]);

  const handleSearch = useCallback(() => {
    if (typeof pagination.goToPage === 'function') {
      pagination.goToPage(1);
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

  const formatDateForInput = (date: Date): string => {
    return new Date(date.getTime() - (date.getTimezoneOffset() * 60000)).toISOString().slice(0, 16);
  };

  // =============================================================================
  // 계산된 값들 (파생 상태)
  // =============================================================================

  // 통계 데이터 보정 로직 (서버 데이터가 0이거나 누락된 경우 현재 리스트를 바탕으로 최소값 보장)
  const computedStats = useMemo(() => {
    const localTotal = pagination.totalCount || alarmEvents.length;
    const localActive = alarmEvents.filter(e => (e.state || '').toLowerCase() === 'active').length;
    const localAck = alarmEvents.filter(e => (e.state || '').toLowerCase() === 'acknowledged').length;
    const localCleared = alarmEvents.filter(e => (e.state || '').toLowerCase() === 'cleared').length;

    if (!statistics) {
      return {
        total: localTotal,
        active: localActive,
        unacknowledged: localActive, // 기본적으로 활성이면 미확인
        acknowledged: localAck,
        cleared: localCleared
      };
    }

    const s = statistics.occurrences;
    return {
      total: Math.max(localTotal, s.total_occurrences || 0),
      active: Math.max(localActive, s.active_alarms || 0),
      unacknowledged: Math.max(localActive, s.unacknowledged_alarms || 0), // 미확인은 최소 현재 보이는 active 개수 이상
      acknowledged: Math.max(localAck, s.acknowledged_alarms || 0),
      cleared: Math.max(localCleared, s.cleared_alarms || 0)
    };
  }, [statistics, alarmEvents, pagination.totalCount]);

  // 알람 처리 핸들러
  const handleAcknowledge = async (id: number) => {
    const isConfirmed = await confirm({
      title: '알람 확인 처리',
      message: '해당 알람을 확인 처리하시겠습니까?',
      confirmText: '확인',
      cancelText: '취소',
      confirmButtonType: 'warning'
    });

    if (!isConfirmed) return;

    try {
      const response = await AlarmApiService.acknowledgeAlarm(id, { comment: 'System Admin acknowledged' });
      if (response.success) {
        await confirm({
          title: '처리 완료',
          message: '알람이 확인 처리되었습니다.',
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'success'
        });

        // 알람 컨텍스트의 활성 알람 개수 명시적 감소
        decrementAlarmCount();

        // 데이터 새로고침 (리스트 및 통계 카드)
        await Promise.all([
          fetchAlarmHistory(),
          fetchStatistics()
        ]);
      } else {
        await confirm({
          title: '처리 실패',
          message: '알람 확인 실패: ' + (response.message || 'Unknown error'),
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      }
    } catch (error) {
      console.error('Error acknowledging alarm:', error);
      await confirm({
        title: '오류 발생',
        message: '알람 확인 중 오류가 발생했습니다.',
        confirmText: '확인',
        showCancelButton: false,
        confirmButtonType: 'danger'
      });
    }
  };

  const handleClear = async (id: number) => {
    const isConfirmed = await confirm({
      title: '알람 해제 처리',
      message: '해당 알람을 해제 처리하시겠습니까?',
      confirmText: '해제',
      cancelText: '취소',
      confirmButtonType: 'danger'
    });

    if (!isConfirmed) return;

    try {
      const response = await AlarmApiService.clearAlarm(id, { comment: 'System Admin cleared' });
      if (response.success) {
        await confirm({
          title: '처리 완료',
          message: '알람이 해제 처리되었습니다.',
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'success'
        });

        // 알람 컨텍스트의 활성 알람 개수 명시적 감소
        decrementAlarmCount();

        // 데이터 새로고침 (리스트 및 통계 카드)
        await Promise.all([
          fetchAlarmHistory(),
          fetchStatistics()
        ]);
      } else {
        await confirm({
          title: '처리 실패',
          message: '알람 해제 실패: ' + (response.message || 'Unknown error'),
          confirmText: '확인',
          showCancelButton: false,
          confirmButtonType: 'danger'
        });
      }
    } catch (error) {
      console.error('Error clearing alarm:', error);
      await confirm({
        title: '오류 발생',
        message: '알람 해제 중 오류가 발생했습니다.',
        confirmText: '확인',
        showCancelButton: false,
        confirmButtonType: 'danger'
      });
    }
  };

  const handleDateChange = (type: 'start' | 'end', value: string) => {
    if (!value) return;
    const newDate = new Date(value);
    handleFilterChange({
      dateRange: {
        ...filters.dateRange,
        [type]: newDate
      }
    });
  };

  const formatValue = (value: any): string => {
    if (value === null || value === undefined) return '-';
    if (typeof value === 'number') {
      return value.toLocaleString(undefined, { maximumFractionDigits: 2 });
    }
    return String(value);
  };

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

  const getSeverityClass = (severity: string): string => {
    return `severity-${severity}`;
  };

  const getStateClass = (state: string): string => {
    return `state-${state}`;
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
  // 렌더링
  // =============================================================================

  return (
    <ManagementLayout>
      <style>{`
        .hover-row.active td {
          background-color: #f0f9ff !important;
        }
        .hover-row:hover td {
          background-color: #f8fafc !important;
        }
        .mgmt-stats-panel {
          margin-bottom: 12px !important;
          gap: 12px !important;
          display: grid !important;
          grid-template-columns: repeat(5, 1fr) !important;
          align-items: stretch !important;
        }
        .mgmt-stats-panel .mgmt-stat-card {
          padding: 16px 20px !important;
          height: 100% !important;
          margin: 0 !important;
        }
        .mgmt-header-info p {
          display: block !important;
          font-size: 14px !important;
          color: #64748b !important;
          margin-top: 4px !important;
        }
        .mgmt-filter-bar {
          padding: 12px 24px !important;
          margin-bottom: 12px !important;
          overflow: visible !important;
        }
        .mgmt-filter-bar .mgmt-input, .mgmt-filter-bar .mgmt-select {
          border-color: #e2e8f0;
          height: 38px !important;
        }
        .table-container {
          padding: 0 !important;
          margin-top: 0 !important;
          border-radius: 12px !important;
        }
      `}</style>
      <PageHeader
        title="알람 이력"
        description="시스템에서 발생한 모든 알람의 이력을 확인하고 분석할 수 있습니다."
        icon="fas fa-history"
        actions={
          <div style={{ display: 'flex', gap: '12px' }}>
            <button
              className="btn-outline"
              onClick={handleRefresh}
              disabled={isInitialLoading}
            >
              <i className={`fas fa-sync-alt ${isBackgroundRefreshing ? 'fa-spin' : ''}`}></i>
              새로고침
            </button>
            <button
              className="btn-primary"
              onClick={handleExportToCSV}
              disabled={!Array.isArray(alarmEvents) || alarmEvents.length === 0}
            >
              <i className="fas fa-download"></i>
              CSV 내보내기
            </button>
          </div>
        }
      />

      {/* 통계 카드 */}
      {statistics && (
        <div className="mgmt-stats-panel" style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '16px', alignItems: 'stretch' }}>
          <StatCard
            title="총 이벤트"
            value={computedStats.total}
            icon="fas fa-list-alt"
            type="primary"
          />
          <StatCard
            title="활성 알람"
            value={computedStats.active}
            icon="fas fa-exclamation-triangle"
            type="error"
          />
          <StatCard
            title="미확인"
            value={computedStats.unacknowledged}
            icon="fas fa-bell"
            type="warning"
          />
          <StatCard
            title="확인됨"
            value={computedStats.acknowledged}
            icon="fas fa-check-circle"
            type="warning"
          />
          <StatCard
            title="해제됨"
            value={computedStats.cleared}
            icon="fas fa-check-double"
            type="success"
          />
        </div>
      )}

      {/* 필터 및 검색 바 (한 줄 레이아웃) */}
      <div className="mgmt-filter-bar" style={{ display: 'flex', alignItems: 'center', gap: '24px', background: 'white', borderRadius: '12px', border: '1px solid #e2e8f0' }}>
        {/* 날짜 범위 선택 */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexShrink: 0 }}>
          <label style={{ fontWeight: 600, fontSize: '14px', color: '#475569', whiteSpace: 'nowrap' }}>기간</label>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <input
              type="datetime-local"
              className="mgmt-input"
              style={{ width: '280px' }}
              value={formatDateForInput(filters.dateRange.start)}
              onChange={(e) => handleDateChange('start', e.target.value)}
            />
            <span style={{ color: '#94a3b8' }}>~</span>
            <input
              type="datetime-local"
              className="mgmt-input"
              style={{ width: '280px' }}
              value={formatDateForInput(filters.dateRange.end)}
              onChange={(e) => handleDateChange('end', e.target.value)}
            />
          </div>
        </div>

        {/* 심각도 필터 */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <label style={{ fontWeight: 600, fontSize: '13px', color: '#475569', whiteSpace: 'nowrap' }}>심각도</label>
          <select
            className="mgmt-select sm"
            style={{ width: '110px' }}
            value={filters.severity}
            onChange={(e) => handleFilterChange({ severity: e.target.value })}
          >
            <option value="all">전체</option>
            <option value="critical">CRITICAL</option>
            <option value="high">HIGH</option>
            <option value="medium">MEDIUM</option>
            <option value="low">LOW</option>
          </select>
        </div>

        {/* 상태 필터 */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <label style={{ fontWeight: 600, fontSize: '13px', color: '#475569', whiteSpace: 'nowrap' }}>상태</label>
          <select
            className="mgmt-select sm"
            style={{ width: '110px' }}
            value={filters.state}
            onChange={(e) => handleFilterChange({ state: e.target.value })}
          >
            <option value="all">전체</option>
            <option value="active">활성</option>
            <option value="acknowledged">확인됨</option>
            <option value="cleared">해제됨</option>
          </select>
        </div>

        {/* 검색 */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flex: 1 }}>
          <div className="mgmt-search-wrapper" style={{ position: 'relative', flex: 1 }}>
            <i className="fas fa-search mgmt-search-icon" style={{ position: 'absolute', left: '12px', top: '50%', transform: 'translateY(-50%)', color: '#94a3b8' }}></i>
            <input
              type="text"
              className="mgmt-input"
              style={{ width: '100%', paddingLeft: '36px' }}
              placeholder="알람명, 디바이스명으로 검색..."
              value={filters.searchTerm}
              onChange={(e) => handleFilterChange({ searchTerm: e.target.value })}
              onKeyDown={(e) => e.key === 'Enter' && handleSearch()}
            />
          </div>
          <button
            className="btn-primary"
            onClick={handleSearch}
            style={{ height: '38px', padding: '0 24px', whiteSpace: 'nowrap', flexShrink: 0 }}
          >
            검색
          </button>
        </div>
      </div>

      {/* 에러 메시지 */}
      {error && (
        <div className="mgmt-card" style={{ padding: '16px', background: '#fef2f2', border: '1px solid #fecaca', color: '#dc2626', marginBottom: '24px' }}>
          <i className="fas fa-exclamation-circle" style={{ marginRight: '8px' }}></i>
          {error}
        </div>
      )}

      {/* 메인 컨텐츠 영역 */}
      <div className="mgmt-card table-container" ref={containerRef}>
        {/* 탭/뷰 모드 전환 */}
        <div className="mgmt-card-header" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <i className="fas fa-list" style={{ color: '#3b82f6' }}></i>
            <h3 style={{ margin: 0, fontSize: '18px', fontWeight: 600 }}>이력 목록 ({computedStats.total})</h3>
          </div>
          <div className="btn-group">
            <button
              className={`btn-outline ${viewMode === 'list' ? 'active' : ''}`}
              onClick={() => setViewMode('list')}
              style={{ padding: '6px 12px' }}
            >
              <i className="fas fa-list" style={{ marginRight: '6px' }}></i>
              목록
            </button>
            <button
              className={`btn-outline ${viewMode === 'timeline' ? 'active' : ''}`}
              onClick={() => setViewMode('timeline')}
              style={{ padding: '6px 12px' }}
            >
              <i className="fas fa-stream" style={{ marginRight: '6px' }}></i>
              타임라인
            </button>
          </div>
        </div>

        {isInitialLoading ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '400px', color: '#64748b' }}>
            <i className="fas fa-spinner fa-spin" style={{ fontSize: '32px', marginBottom: '16px', color: '#3b82f6' }}></i>
            <div>데이터를 불러오는 중입니다...</div>
          </div>
        ) : (
          <>
            <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
              {viewMode === 'list' ? (
                <div style={{ overflowX: 'auto', overflowY: 'auto' }}>
                  <table className="mgmt-table">
                    <thead>
                      <tr>
                        <th style={{ width: '60px' }}>ID</th>
                        <th style={{ width: '100px' }}>심각도</th>
                        <th style={{ width: '180px' }}>디바이스 / 포인트</th>
                        <th>메시지</th>
                        <th style={{ width: '100px' }}>상태</th>
                        <th style={{ width: '160px' }}>발생시간</th>
                        <th style={{ width: '120px' }}>지속시간</th>
                        <th style={{ width: '80px', textAlign: 'center' }}>액션</th>
                      </tr>
                    </thead>
                    <tbody>
                      {(!alarmEvents || alarmEvents.length === 0) ? (
                        <tr>
                          <td colSpan={8} style={{ textAlign: 'center', padding: '64px 0', color: '#64748b' }}>
                            <i className="fas fa-inbox" style={{ fontSize: '48px', marginBottom: '16px', color: '#cbd5e1' }}></i>
                            <div style={{ fontSize: '16px' }}>검색 결과가 없습니다.</div>
                          </td>
                        </tr>
                      ) : (
                        alarmEvents.map((event) => {
                          if (!event) return null;
                          const occurrenceTime = event.occurrence_time || new Date().toISOString();
                          const severityClass = (event.severity || '').toLowerCase() === 'critical' ? 'critical' :
                            (event.severity || '').toLowerCase() === 'high' ? 'high' :
                              (event.severity || '').toLowerCase() === 'medium' ? 'medium' : 'low';
                          const state = (event.state || 'active').toLowerCase();

                          return (
                            <tr
                              key={event.id}
                              className={`hover-row ${activeRowId === event.id ? 'active' : ''}`}
                              onClick={() => setActiveRowId(event.id)}
                              style={{ cursor: 'pointer' }}
                            >
                              <td style={{
                                position: 'relative',
                                borderLeft: '4px solid #3b82f6',
                                paddingLeft: '24px',
                                fontWeight: 600,
                                color: activeRowId === event.id ? '#3b82f6' : '#1e293b'
                              }}>
                                # {event.id}
                              </td>
                              <td>
                                <span className={`status-pill ${severityClass}`}>
                                  {(event.severity || 'UNKNOWN').toUpperCase()}
                                </span>
                              </td>
                              <td>
                                <div style={{ fontWeight: 600, color: '#1e293b' }}>{event.device_name || 'N/A'}</div>
                                <div style={{ fontSize: '12px', color: '#64748b' }}>{event.data_point_name || event.rule_name || 'N/A'}</div>
                              </td>
                              <td>
                                <div style={{ color: '#334155' }}>{event.alarm_message || 'No description'}</div>
                                <div className="text-monospace" style={{ fontSize: '11px', color: '#3b82f6', marginTop: '4px' }}>
                                  값: {formatValue(event.trigger_value)}
                                </div>
                              </td>
                              <td>
                                <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                                  <i
                                    className={`fas ${state === 'active' ? 'fa-exclamation-circle' : 'fa-check-circle'}`}
                                    style={{ color: state === 'active' ? '#ef4444' : '#22c55e' }}
                                  ></i>
                                  <span style={{ fontWeight: 500 }}>{getStatusText(event.state)}</span>
                                </div>
                              </td>
                              <td className="text-monospace">
                                {new Date(occurrenceTime).toLocaleString('ko-KR')}
                              </td>
                              <td className="text-monospace">
                                {formatDuration(occurrenceTime, event.cleared_time || event.acknowledged_time)}
                              </td>
                              <td style={{ textAlign: 'center' }}>
                                <div style={{ display: 'flex', justifyContent: 'center', gap: '8px' }}>
                                  {state === 'active' && (
                                    <button
                                      className="btn-icon"
                                      style={{ color: '#f59e0b', background: '#fffbeb', border: '1px solid #fde68a', borderRadius: '6px', width: '32px', height: '32px' }}
                                      onClick={(e) => { e.stopPropagation(); handleAcknowledge(event.id); }}
                                      title="알람 확인 (확인 후 해제 가능)"
                                    >
                                      <i className="fas fa-check"></i>
                                    </button>
                                  )}
                                  {state === 'acknowledged' && (
                                    <button
                                      className="btn-icon"
                                      style={{ color: '#ef4444', background: '#fef2f2', border: '1px solid #fee2e2', borderRadius: '6px', width: '32px', height: '32px' }}
                                      onClick={(e) => { e.stopPropagation(); handleClear(event.id); }}
                                      title="알람 해제"
                                    >
                                      <i className="fas fa-times-circle"></i>
                                    </button>
                                  )}
                                  <button
                                    className="btn-icon"
                                    style={{ background: '#f8fafc', border: '1px solid #e2e8f0', borderRadius: '6px', width: '32px', height: '32px' }}
                                    onClick={(e) => { e.stopPropagation(); handleViewDetails(event); }}
                                    title="상세 보기"
                                  >
                                    <i className="fas fa-eye"></i>
                                  </button>
                                </div>
                              </td>
                            </tr>
                          );
                        })
                      )}
                    </tbody>
                  </table>
                </div>
              ) : (
                /* 타임라인 뷰 리포매팅 */
                <div style={{ padding: '24px', maxHeight: '600px', overflowY: 'auto' }}>
                  <div style={{ position: 'relative', borderLeft: '2px solid #e2e8f0', marginLeft: '16px', paddingLeft: '24px' }}>
                    {(!alarmEvents || alarmEvents.length === 0) ? (
                      <div style={{ textAlign: 'center', padding: '48px 0', color: '#64748b' }}>
                        검색 결과가 없습니다.
                      </div>
                    ) : (
                      alarmEvents.map((event) => {
                        if (!event) return null;
                        const occurrenceTime = event.occurrence_time || new Date().toISOString();
                        const state = (event.state || 'active').toLowerCase();
                        const severity = (event.severity || 'LOW').toUpperCase();

                        return (
                          <div key={event.id} style={{ marginBottom: '32px', position: 'relative' }}>
                            <div style={{
                              position: 'absolute',
                              left: '-33px',
                              top: '0',
                              width: '16px',
                              height: '16px',
                              borderRadius: '50%',
                              background: severity === 'CRITICAL' ? '#ef4444' : state === 'active' ? '#3b82f6' : '#cbd5e1',
                              border: '4px solid white',
                              boxShadow: '0 0 0 1px #e2e8f0'
                            }}></div>
                            <div style={{ background: 'white', padding: '16px', borderRadius: '8px', border: '1px solid #e2e8f0', boxShadow: '0 1px 2px 0 rgb(0 0 0 / 0.05)' }}>
                              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
                                <span style={{ fontSize: '12px', color: '#64748b', fontWeight: 600 }}>
                                  {new Date(occurrenceTime).toLocaleString('ko-KR')}
                                </span>
                                <span className={`status-pill ${severity.toLowerCase()}`}>
                                  {severity}
                                </span>
                              </div>
                              <div style={{ fontWeight: 600, fontSize: '16px', marginBottom: '4px' }}>{event.alarm_message || 'No message'}</div>
                              <div style={{ fontSize: '13px', color: '#64748b' }}>
                                {event.device_name || 'Unknown Device'} &bull; {event.rule_name || 'Unknown Rule'}
                              </div>
                              <div style={{ marginTop: '12px', textAlign: 'right' }}>
                                <button
                                  className="btn-link"
                                  onClick={() => handleViewDetails(event)}
                                  style={{ fontSize: '12px' }}
                                >
                                  상세 보기 <i className="fas fa-chevron-right" style={{ fontSize: '10px' }}></i>
                                </button>
                              </div>
                            </div>
                          </div>
                        );
                      })
                    )}
                  </div>
                </div>
              )}
            </div>

            {/* 페이지네이션 (전체 뷰 공통) */}
            {Array.isArray(alarmEvents) && alarmEvents.length > 0 && (
              <div style={{ padding: '16px 24px', borderTop: '1px solid #e2e8f0', background: 'white' }}>
                <Pagination
                  current={pagination.currentPage}
                  total={pagination.totalCount}
                  pageSize={pagination.pageSize}
                  onChange={(page, size) => {
                    pagination.goToPage(page);
                    if (size && size !== pagination.pageSize) {
                      pagination.changePageSize(size);
                    }
                  }}
                  showSizeChanger={true}
                />
              </div>
            )}
          </>
        )}
      </div>

      {/* 상세 보기 모달 */}
      {showDetailsModal && selectedEvent && (
        <div className="modal-overlay" style={{ position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, background: 'rgba(0,0,0,0.5)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 1000 }}>
          <div className="modal-content" style={{ background: 'white', borderRadius: '12px', width: '90%', maxWidth: '500px', maxHeight: '90vh', overflowY: 'auto', boxShadow: '0 20px 25px -5px rgba(0,0,0,0.1)' }}>
            <div className="modal-header" style={{ padding: '16px 20px', borderBottom: '1px solid #e2e8f0', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <h3 style={{ margin: 0, fontSize: '18px', fontWeight: 600 }}>이력 상세 정보</h3>
              <button onClick={() => setShowDetailsModal(false)} style={{ background: 'none', border: 'none', fontSize: '20px', cursor: 'pointer', color: '#64748b' }}>&times;</button>
            </div>

            <div className="modal-body" style={{ padding: '20px' }}>
              <div style={{ marginBottom: '20px' }}>
                <h4 style={{ margin: '0 0 12px 0', fontSize: '14px', fontWeight: 600, color: '#3b82f6', borderLeft: '3px solid #3b82f6', paddingLeft: '8px' }}>기본 정보</h4>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                  <div>
                    <label style={{ display: 'block', fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>알람 ID</label>
                    <div style={{ fontWeight: 600 }}>#{selectedEvent.id}</div>
                  </div>
                  <div>
                    <label style={{ display: 'block', fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>심각도</label>
                    <span className={`status-pill ${selectedEvent.severity === 'critical' || selectedEvent.severity === 'high' ? 'error' : 'warning'}`} style={{ display: 'inline-block' }}>
                      {selectedEvent.severity?.toUpperCase()}
                    </span>
                  </div>
                  <div style={{ gridColumn: '1 / -1' }}>
                    <label style={{ display: 'block', fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>디바이스 / 규칙</label>
                    <div style={{ fontWeight: 500 }}>
                      <strong style={{ color: '#1e293b' }}>{selectedEvent.device_name}</strong>
                      <span style={{ margin: '0 8px', color: '#cbd5e1' }}>/</span>
                      <span style={{ color: '#475569' }}>{selectedEvent.rule_name}</span>
                    </div>
                  </div>
                </div>
              </div>

              <div>
                <h4 style={{ margin: '0 0 12px 0', fontSize: '14px', fontWeight: 600, color: '#3b82f6', borderLeft: '3px solid #3b82f6', paddingLeft: '8px' }}>발생 및 조치 이력</h4>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                  <div style={{ display: 'flex', gap: '12px' }}>
                    <div style={{ minWidth: '60px', fontSize: '13px', color: '#64748b', paddingTop: '2px' }}>발생</div>
                    <div style={{ flex: 1 }}>
                      <div style={{ fontSize: '14px', fontWeight: 500, marginBottom: '4px' }}>{new Date(selectedEvent.occurrence_time).toLocaleString('ko-KR')}</div>
                      <div style={{ fontSize: '13px', color: '#475569', background: '#f8fafc', padding: '8px 12px', borderRadius: '6px', border: '1px solid #e2e8f0' }}>
                        {selectedEvent.alarm_message}
                        {selectedEvent.trigger_value && <div style={{ marginTop: '4px', fontSize: '12px', color: '#64748b' }}>값: {selectedEvent.trigger_value}</div>}
                      </div>
                    </div>
                  </div>

                  {selectedEvent.acknowledged_time && (
                    <div style={{ display: 'flex', gap: '12px' }}>
                      <div style={{ minWidth: '60px', fontSize: '13px', color: '#64748b', paddingTop: '2px' }}>확인</div>
                      <div style={{ flex: 1 }}>
                        <div style={{ fontSize: '14px', fontWeight: 500, marginBottom: '4px' }}>
                          {new Date(selectedEvent.acknowledged_time).toLocaleString('ko-KR')}
                        </div>
                        <div style={{ fontSize: '13px', color: '#64748b', marginBottom: '4px' }}>
                          확인자: {selectedEvent.acknowledged_by_company ? `${selectedEvent.acknowledged_by_company} / ` : ''}{selectedEvent.acknowledged_by_name || selectedEvent.acknowledged_by || '-'}
                        </div>
                        {selectedEvent.acknowledge_comment && (
                          <div style={{ fontSize: '13px', color: '#475569', background: '#fffbeb', padding: '8px 12px', borderRadius: '6px', border: '1px solid #fcd34d', whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>
                            확인 메모: {selectedEvent.acknowledge_comment}
                          </div>
                        )}
                      </div>
                    </div>
                  )}

                  {selectedEvent.cleared_time && (
                    <div style={{ display: 'flex', gap: '12px' }}>
                      <div style={{ minWidth: '60px', fontSize: '13px', color: '#64748b', paddingTop: '2px' }}>해제</div>
                      <div style={{ flex: 1 }}>
                        <div style={{ fontSize: '14px', fontWeight: 500, marginBottom: '4px' }}>
                          {new Date(selectedEvent.cleared_time).toLocaleString('ko-KR')}
                        </div>
                        <div style={{ fontSize: '13px', color: '#64748b', marginBottom: '4px' }}>
                          해제자: {selectedEvent.cleared_by_company ? `${selectedEvent.cleared_by_company} / ` : ''}{selectedEvent.cleared_by_name || selectedEvent.cleared_by || '-'}
                        </div>
                        {selectedEvent.clear_comment && (
                          <div style={{ fontSize: '13px', color: '#475569', background: '#f0fdf4', padding: '8px 12px', borderRadius: '6px', border: '1px solid #86efac', whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>
                            해제 메모: {selectedEvent.clear_comment}
                          </div>
                        )}
                      </div>
                    </div>
                  )}
                </div>
              </div>
            </div>

            <div className="modal-footer" style={{ padding: '16px 20px', borderTop: '1px solid #e2e8f0', display: 'flex', justifyContent: 'flex-end', background: '#f8fafc', borderRadius: '0 0 12px 12px' }}>
              <button
                onClick={() => setShowDetailsModal(false)}
                style={{ padding: '8px 16px', background: 'white', border: '1px solid #cbd5e1', borderRadius: '6px', cursor: 'pointer', fontSize: '13px', fontWeight: 500, color: '#475569' }}
              >
                닫기
              </button>
            </div>
          </div>
        </div>
      )}
    </ManagementLayout>
  );
};

export default AlarmHistory;