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
import { FilterBar } from '../components/common/FilterBar';
import '../styles/management.css';
import '../styles/pagination.css';
import '../styles/alarm-history.css';
import AlarmActionModal from '../components/modals/AlarmActionModal';
import AlarmHistoryDetailModal from '../components/modals/AlarmHistoryDetailModal';

// ─── 제어 감사 로그 타입 ─────────────────────────────────────────────
interface ControlLog {
  id: number;
  request_id: string;
  username?: string;
  device_name?: string;
  point_name?: string;
  protocol_type?: string;
  address?: string;
  old_value?: string;
  requested_value: string;
  delivery_status: 'pending' | 'delivered' | 'no_collector';
  execution_result: 'pending' | 'protocol_success' | 'protocol_failure' | 'protocol_async' | 'timeout' | 'failure';
  verification_result: 'pending' | 'verified' | 'unverified' | 'skipped';
  final_status: 'pending' | 'success' | 'partial' | 'failure' | 'timeout';
  linked_alarm_id?: number;
  requested_at: string;
  executed_at?: string;
  duration_ms?: number;
}

// 화면 크기 감지 훅
function useMediaQuery(query: string): boolean {
  const [matches, setMatches] = useState(() => window.matchMedia(query).matches);
  useEffect(() => {
    const mql = window.matchMedia(query);
    const handler = (e: MediaQueryListEvent) => setMatches(e.matches);
    mql.addEventListener('change', handler);
    return () => mql.removeEventListener('change', handler);
  }, [query]);
  return matches;
}

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
  const { decrementAlarmCount, refreshAlarmCount } = useAlarmContext();
  const isSmallScreen = useMediaQuery('(max-width: 1600px)');
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
  const [showActionModal, setShowActionModal] = useState(false);
  const [actionType, setActionType] = useState<'acknowledge' | 'clear'>('acknowledge');
  const [targetAlarmId, setTargetAlarmId] = useState<number | null>(null);
  const [actionLoading, setActionLoading] = useState(false);
  const [activeRowId, setActiveRowId] = useState<number | null>(null);

  // ── 탭 상태 (알람이력 / 제어이력) ────────────────────────────
  const [activeTab, setActiveTab] = useState<'alarm' | 'control'>('alarm');
  const [controlLogs, setControlLogs] = useState<ControlLog[]>([]);
  const [controlLoading, setControlLoading] = useState(false);
  const [controlTotal, setControlTotal] = useState(0);
  const [controlPage, setControlPage] = useState(1);
  const CONTROL_PAGE_SIZE = 50;

  // 필터 상태
  const [filters, setFilters] = useState<FilterOptions>({
    dateRange: {
      start: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000), // 7일 전으로 변경
      end: new Date(Date.now() + 60 * 60 * 1000) // Now + 1 hour for visibility buffer
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

  // Auto-refresh
  const AUTO_REFRESH_INTERVAL = 10;
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [countdown, setCountdown] = useState(AUTO_REFRESH_INTERVAL);
  const countdownRef = useRef<ReturnType<typeof setInterval> | null>(null);

  // =============================================================================
  // 원본 인라인 스타일 객체들 (중요한 레이아웃 유지)
  // =============================================================================


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

  // ── 제어이력 API 호출 ─────────────────────────────────────────
  const fetchControlLogs = useCallback(async (page = 1) => {
    setControlLoading(true);
    try {
      const params = new URLSearchParams({ page: String(page), limit: String(CONTROL_PAGE_SIZE) });
      const res = await fetch(`/api/control-logs?${params}`, {
        headers: { Authorization: `Bearer ${localStorage.getItem('token') || ''}` }
      });
      const json = await res.json();
      if (json.success) {
        setControlLogs(json.data || []);
        setControlTotal(json.pagination?.total || 0);
      }
    } catch (err) {
      console.error('[ControlLogs] 조회 실패:', err);
    } finally {
      setControlLoading(false);
    }
  }, [CONTROL_PAGE_SIZE]);

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
    // Modal state is controlled by whether selectedEvent is non-null in the new component pattern, 
    // but here we keep showDetailsModal for explicit control if needed, or simplify.
    // The current pattern uses showDetailsModal boolean.
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

  const formatDateOnly = (date: Date): string => {
    return new Date(date.getTime() - (date.getTimezoneOffset() * 60000)).toISOString().slice(0, 10);
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
  const handleAcknowledge = (id: number) => {
    const event = alarmEvents.find(e => e.id === id);
    if (!event) return;

    setTargetAlarmId(id);
    setActionType('acknowledge');
    setSelectedEvent(event);
    setShowActionModal(true);
  };

  const handleClear = (id: number) => {
    const event = alarmEvents.find(e => e.id === id);
    if (!event) return;

    setTargetAlarmId(id);
    setActionType('clear');
    setSelectedEvent(event);
    setShowActionModal(true);
  };

  const onConfirmAction = async (comment: string) => {
    if (!targetAlarmId) return;

    setActionLoading(true);
    try {
      let response;
      if (actionType === 'acknowledge') {
        response = await AlarmApiService.acknowledgeAlarm(targetAlarmId, { comment });
      } else {
        response = await AlarmApiService.clearAlarm(targetAlarmId, { comment });
      }

      if (response.success) {
        // 전역 뱃지 재조회 (단순 -1 대신 서버에서 정확한 미확인 개수 조회)
        await refreshAlarmCount();
        await Promise.all([
          fetchAlarmHistory(),
          fetchStatistics()
        ]);
        setShowActionModal(false);
      } else {
        alert(`${actionType === 'acknowledge' ? '확인' : '해제'} 처리 실패: ${response.message}`);
      }
    } catch (err) {
      alert(`${actionType === 'acknowledge' ? '확인' : '해제'} 처리 중 오류가 발생했습니다.`);
    } finally {
      setActionLoading(false);
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
    if (countdownRef.current) clearInterval(countdownRef.current);
    if (!autoRefresh || !hasInitialLoad) { setCountdown(AUTO_REFRESH_INTERVAL); return; }

    setCountdown(AUTO_REFRESH_INTERVAL);
    countdownRef.current = setInterval(() => {
      setCountdown(prev => {
        if (prev <= 1) {
          handleRefresh();
          return AUTO_REFRESH_INTERVAL;
        }
        return prev - 1;
      });
    }, 1000);

    return () => { if (countdownRef.current) clearInterval(countdownRef.current); };
  }, [autoRefresh, hasInitialLoad, handleRefresh]);

  // =============================================================================
  // 렌더링
  // =============================================================================

  return (
    <ManagementLayout className="page-alarm-history">
      <PageHeader
        title="알람 이력"
        description="시스템에서 발생한 모든 알람의 이력을 확인하고 분석할 수 있습니다."
        icon="fas fa-history"
        actions={
          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
            {/* Auto-refresh pill */}
            <div style={{
              display: 'flex', alignItems: 'center',
              background: autoRefresh ? 'var(--primary-50, #eff6ff)' : 'var(--neutral-100)',
              border: `1px solid ${autoRefresh ? 'var(--primary-200, #bfdbfe)' : 'var(--neutral-200)'}`,
              borderRadius: '20px', padding: '4px 6px 4px 10px', gap: '6px',
              transition: 'all 0.2s ease',
            }}>
              <span style={{ fontSize: '12px', fontVariantNumeric: 'tabular-nums', color: autoRefresh ? 'var(--primary-600, #2563eb)' : 'var(--neutral-400)', minWidth: '36px', fontWeight: 500 }}>
                {autoRefresh
                  ? <><i className="fas fa-circle-notch fa-spin" style={{ marginRight: '4px', fontSize: '10px' }}></i>{countdown}s</>
                  : <><i className="fas fa-pause" style={{ marginRight: '4px', fontSize: '10px', color: 'var(--neutral-400)' }}></i>정지</>}
              </span>
              <button
                style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', width: '22px', height: '22px', borderRadius: '50%', border: 'none', cursor: 'pointer', background: autoRefresh ? 'var(--primary-100, #dbeafe)' : 'var(--neutral-200)', color: autoRefresh ? 'var(--primary-600, #2563eb)' : 'var(--neutral-500)', fontSize: '9px', flexShrink: 0, transition: 'all 0.15s ease' }}
                onClick={() => setAutoRefresh(v => !v)}
                title={autoRefresh ? '자동 새로고침 중지' : '자동 새로고침 시작'}
              >
                <i className={`fas ${autoRefresh ? 'fa-pause' : 'fa-play'}`}></i>
              </button>
            </div>
            <button
              className="mgmt-btn mgmt-btn-outline"
              onClick={handleRefresh}
              disabled={isInitialLoading}
            >
              <i className={`fas fa-sync-alt ${isBackgroundRefreshing ? 'fa-spin' : ''}`}></i>
              새로고침
            </button>
            <button
              className="mgmt-btn mgmt-btn-primary"
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
      <div className="mgmt-stats-panel" style={{ gridTemplateColumns: 'repeat(5, 1fr)', gap: '12px', alignItems: 'stretch', marginBottom: '8px' }}>
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

      <FilterBar
        searchTerm={filters.searchTerm}
        onSearchChange={(v) => handleFilterChange({ searchTerm: v })}
        activeFilterCount={(filters.severity !== 'all' ? 1 : 0) + (filters.state !== 'all' ? 1 : 0) + (filters.searchTerm ? 1 : 0)}
        onReset={() => setFilters({
          dateRange: {
            start: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000),
            end: new Date(Date.now() + 60 * 60 * 1000)
          },
          severity: 'all',
          state: 'all',
          searchTerm: ''
        })}
        filters={[
          {
            label: '심각도',
            value: filters.severity,
            onChange: (v) => handleFilterChange({ severity: v }),
            options: [
              { label: '전체', value: 'all' },
              { label: 'CRITICAL', value: 'critical' },
              { label: 'HIGH', value: 'high' },
              { label: 'MEDIUM', value: 'medium' },
              { label: 'LOW', value: 'low' }
            ]
          },
          {
            label: '상태',
            value: filters.state,
            onChange: (v) => handleFilterChange({ state: v }),
            options: [
              { label: '전체', value: 'all' },
              { label: '활성', value: 'active' },
              { label: '확인됨', value: 'acknowledged' },
              { label: '해제됨', value: 'cleared' }
            ]
          }
        ]}
        leftActions={
          <div className="mgmt-filter-group">
            <label>기간 설정</label>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <input
                type={isSmallScreen ? 'date' : 'datetime-local'}
                className="mgmt-input alarm-date-input"
                value={isSmallScreen ? formatDateOnly(filters.dateRange.start) : formatDateForInput(filters.dateRange.start)}
                onChange={(e) => handleDateChange('start', e.target.value)}
              />
              <span style={{ color: 'var(--neutral-400)' }}>~</span>
              <input
                type={isSmallScreen ? 'date' : 'datetime-local'}
                className="mgmt-input alarm-date-input"
                value={isSmallScreen ? formatDateOnly(filters.dateRange.end) : formatDateForInput(filters.dateRange.end)}
                onChange={(e) => handleDateChange('end', e.target.value)}
              />
            </div>
          </div>
        }
        rightActions={
          <button
            className="mgmt-btn mgmt-btn-primary"
            onClick={handleSearch}
            style={{ height: '36px' }}
          >
            조회
          </button>
        }
      />

      {/* 에러 메시지 */}
      {error && (
        <div className="mgmt-card" style={{ padding: '16px', background: '#fef2f2', border: '1px solid #fecaca', color: '#dc2626', marginBottom: '24px' }}>
          <i className="fas fa-exclamation-circle" style={{ marginRight: '8px' }}></i>
          {error}
        </div>
      )}

      {/* 메인 컨텐츠 영역 */}
      <div className="mgmt-card table-container" style={{ flex: 1, display: 'flex', flexDirection: 'column', minHeight: '500px' }}>
        {/* ── 상위 탭 전환 (알람이력 / 제어이력) ── */}
        <div style={{ display: 'flex', alignItems: 'center', padding: '0 24px', borderBottom: '2px solid var(--neutral-100)', gap: 0 }}>
          <button
            onClick={() => setActiveTab('alarm')}
            style={{
              padding: '14px 20px', border: 'none', background: 'none', cursor: 'pointer',
              fontWeight: activeTab === 'alarm' ? 700 : 400,
              color: activeTab === 'alarm' ? 'var(--primary-600, #2563eb)' : 'var(--neutral-500)',
              borderBottom: activeTab === 'alarm' ? '2px solid var(--primary-600, #2563eb)' : '2px solid transparent',
              marginBottom: '-2px', fontSize: '14px', display: 'flex', alignItems: 'center', gap: '6px'
            }}
          >
            <i className="fas fa-bell" />
            알람이력 ({computedStats.total})
          </button>
          <button
            onClick={() => {
              setActiveTab('control');
              if (controlLogs.length === 0) fetchControlLogs(1);
            }}
            style={{
              padding: '14px 20px', border: 'none', background: 'none', cursor: 'pointer',
              fontWeight: activeTab === 'control' ? 700 : 400,
              color: activeTab === 'control' ? 'var(--primary-600, #2563eb)' : 'var(--neutral-500)',
              borderBottom: activeTab === 'control' ? '2px solid var(--primary-600, #2563eb)' : '2px solid transparent',
              marginBottom: '-2px', fontSize: '14px', display: 'flex', alignItems: 'center', gap: '6px'
            }}
          >
            <i className="fas fa-sliders-h" />
            제어이력 {controlTotal > 0 ? `(${controlTotal})` : ''}
          </button>
        </div>

        {/* 알람이력 탭: 기존 탭/뷰 모드 전환 */}
        {activeTab === 'alarm' && (
          <div className="mgmt-header" style={{ border: 'none', background: 'transparent' }}>
            <div className="mgmt-header-info">
              <h3 className="mgmt-title" style={{ fontSize: '18px' }}>
                <i className="fas fa-list-ul" style={{ color: 'var(--primary-500)' }}></i>
                이력 내역 ({computedStats.total})
              </h3>
            </div>
            <div className="view-toggle">
              <button
                className={`mgmt-btn-icon ${viewMode === 'list' ? 'active' : ''}`}
                onClick={() => setViewMode('list')}
                title="목록 보기"
              >
                <i className="fas fa-list"></i>
              </button>
              <button
                className={`mgmt-btn-icon ${viewMode === 'timeline' ? 'active' : ''}`}
                onClick={() => setViewMode('timeline')}
                title="타임라인 보기"
              >
                <i className="fas fa-stream"></i>
              </button>
            </div>
          </div>
        )}

        {isInitialLoading ? (
          <div className="mgmt-loading">
            <i className="fas fa-spinner fa-spin"></i>
            <p>데이터를 불러오는 중입니다...</p>
          </div>
        ) : (
          <>
            <div className="mgmt-table-wrapper" ref={containerRef} style={{ flex: 1 }}>
              {viewMode === 'list' ? (
                <table className="mgmt-table">
                  <thead>
                    <tr>
                      <th style={{ width: '100px' }}>ID</th>
                      <th style={{ width: '120px', textAlign: 'center' }}>심각도</th>
                      <th style={{ width: '180px' }}>디바이스 / 포인트</th>
                      <th>메시지</th>
                      <th style={{ width: '120px', textAlign: 'center' }}>상태</th>
                      <th style={{ width: '200px', whiteSpace: 'nowrap' }}>발생시간</th>
                      <th style={{ width: '90px' }}>지속시간</th>
                      <th style={{ width: '60px', textAlign: 'center' }}>액션</th>
                    </tr>
                  </thead>
                  <tbody>
                    {(!alarmEvents || alarmEvents.length === 0) ? (
                      <tr>
                        <td colSpan={8} className="text-center" style={{ padding: '80px 0' }}>
                          <div className="text-muted">
                            <i className="fas fa-inbox" style={{ fontSize: '48px', marginBottom: '16px', display: 'block', opacity: 0.3 }}></i>
                            검색 결과가 없습니다.
                          </div>
                        </td>
                      </tr>
                    ) : (
                      alarmEvents.map((event) => {
                        if (!event) return null;
                        const occurrenceTime = event.occurrence_time || new Date().toISOString();
                        const severityClass = (event.severity || '').toLowerCase();
                        const state = (event.state || 'active').toLowerCase();

                        return (
                          <tr key={event.id} className={activeRowId === event.id ? 'active' : ''} onClick={() => setActiveRowId(event.id)}>
                            <td className="font-bold text-primary" style={{ whiteSpace: 'nowrap' }}># {event.id}</td>
                            <td style={{ textAlign: 'center' }}>
                              <span className={`status-pill ${severityClass}`}>
                                {(event.severity || 'UNKNOWN').toUpperCase()}
                              </span>
                            </td>
                            <td>
                              <div className="font-semibold text-neutral-900">{event.device_name || 'N/A'}</div>
                              <div className="text-xs text-neutral-500">{event.data_point_name || event.rule_name || 'N/A'}</div>
                            </td>
                            <td>
                              <div className="text-neutral-800">{event.alarm_message || '메시지 없음'}</div>
                              {event.trigger_value !== undefined && (
                                <div className="text-xs text-primary-600 font-medium" style={{ marginTop: '2px' }}>
                                  현재값: {formatValue(event.trigger_value)}
                                </div>
                              )}
                            </td>
                            <td style={{ textAlign: 'center' }}>
                              <div className="flex items-center justify-center gap-2">
                                <i className={getStatusIcon(state)} style={{ color: getStatusColor(state), fontSize: '14px' }}></i>
                                <span className="font-medium">{getStatusText(state)}</span>
                              </div>
                            </td>
                            <td className="text-neutral-600" style={isSmallScreen ? undefined : { whiteSpace: 'nowrap' }}>
                              {isSmallScreen ? (
                                <>
                                  <div>{new Date(occurrenceTime).toLocaleDateString('ko-KR')}</div>
                                  <div className="text-xs">{new Date(occurrenceTime).toLocaleTimeString('ko-KR')}</div>
                                </>
                              ) : (
                                new Date(occurrenceTime).toLocaleString('ko-KR')
                              )}
                            </td>
                            <td className="text-neutral-600">
                              {formatDuration(occurrenceTime, event.cleared_time || event.acknowledged_time)}
                            </td>
                            <td className="text-center">
                              <div className="flex justify-center gap-2">
                                {state === 'active' && (
                                  <button
                                    className="mgmt-btn-icon warning"
                                    onClick={(e) => { e.stopPropagation(); handleAcknowledge(event.id); }}
                                    title="알람 확인"
                                  >
                                    <i className="fas fa-check"></i>
                                  </button>
                                )}
                                {state === 'acknowledged' && (
                                  <button
                                    className="mgmt-btn-icon error"
                                    onClick={(e) => { e.stopPropagation(); handleClear(event.id); }}
                                    title="알람 해제"
                                  >
                                    <i className="fas fa-times"></i>
                                  </button>
                                )}
                                <button
                                  className="mgmt-btn-icon functional"
                                  onClick={(e) => { e.stopPropagation(); handleViewDetails(event); }}
                                  title="상세 보기"
                                >
                                  <i className="fas fa-info-circle"></i>
                                </button>
                              </div>
                            </td>
                          </tr>
                        );
                      })
                    )}
                  </tbody>
                </table>
              ) : (
                <div style={{ padding: '32px', overflowY: 'auto' }}>
                  <div style={{ position: 'relative', borderLeft: '2px solid var(--neutral-200)', marginLeft: '16px', paddingLeft: '32px' }}>
                    {(!alarmEvents || alarmEvents.length === 0) ? (
                      <div className="text-center text-muted" style={{ padding: '40px' }}>검색 결과가 없습니다.</div>
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
                              left: '-41px',
                              top: '0',
                              width: '16px',
                              height: '16px',
                              borderRadius: '50%',
                              background: severity === 'CRITICAL' ? 'var(--error-500)' : state === 'active' ? 'var(--primary-500)' : 'var(--neutral-300)',
                              border: '4px solid white',
                              boxShadow: '0 0 0 1px var(--neutral-200)'
                            }}></div>
                            <div className="mgmt-card" style={{ padding: '20px', borderRadius: '12px' }}>
                              <div className="flex justify-between items-start mb-2">
                                <span className="text-xs font-semibold text-neutral-500">
                                  {new Date(occurrenceTime).toLocaleString('ko-KR')}
                                </span>
                                <span className={`status-pill ${severity.toLowerCase()}`}>
                                  {severity}
                                </span>
                              </div>
                              <div className="font-bold text-lg mb-1">{event.alarm_message || '메시지 없음'}</div>
                              <div className="text-sm text-neutral-600">
                                {event.device_name || '디바이스 정보 없음'} &bull; {event.rule_name || '규칙 정보 없음'}
                              </div>
                              <div className="flex justify-end mt-4">
                                <button className="mgmt-btn-link" onClick={() => handleViewDetails(event)}>
                                  상세 내용 보기 <i className="fas fa-chevron-right"></i>
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

            {/* 페이지네이션 */}
            <div className="mgmt-footer" style={{ padding: '16px 24px', borderTop: '1px solid var(--neutral-200)', background: 'white' }}>
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
          </>
        )}

        {/* ── 제어이력 탭 ── */}
        {activeTab === 'control' && (
          <>
            {controlLoading ? (
              <div className="mgmt-loading">
                <i className="fas fa-spinner fa-spin"></i>
                <p>제어 이력을 불러오는 중...</p>
              </div>
            ) : (
              <div className="mgmt-table-wrapper" style={{ flex: 1 }}>
                <table className="mgmt-table">
                  <thead>
                    <tr>
                      <th style={{ width: '50px' }}>ID</th>
                      <th style={{ width: '90px' }}>사용자</th>
                      <th style={{ width: '150px' }}>디바이스 / 포인트</th>
                      <th style={{ width: '90px', textAlign: 'center' }}>변경값</th>
                      <th style={{ width: '110px', textAlign: 'center' }}>전달</th>
                      <th style={{ width: '110px', textAlign: 'center' }}>실행</th>
                      <th style={{ width: '110px', textAlign: 'center' }}>반영</th>
                      <th style={{ width: '80px', textAlign: 'center' }}>최종</th>
                      <th style={{ width: '80px', textAlign: 'center' }}>알람</th>
                      <th style={{ width: '150px' }}>요청시간</th>
                    </tr>
                  </thead>
                  <tbody>
                    {controlLogs.length === 0 ? (
                      <tr>
                        <td colSpan={10} className="text-center" style={{ padding: '80px 0' }}>
                          <div className="text-muted">
                            <i className="fas fa-inbox" style={{ fontSize: '48px', marginBottom: '16px', display: 'block', opacity: 0.3 }}></i>
                            제어 이력이 없습니다.
                          </div>
                        </td>
                      </tr>
                    ) : (
                      controlLogs.map((log) => {
                        const deliveryColor = log.delivery_status === 'delivered' ? '#22c55e' : log.delivery_status === 'no_collector' ? '#ef4444' : '#f59e0b';
                        const deliveryIcon = log.delivery_status === 'delivered' ? 'fa-check-circle' : log.delivery_status === 'no_collector' ? 'fa-times-circle' : 'fa-clock';
                        const deliveryText = log.delivery_status === 'delivered' ? '전달됨' : log.delivery_status === 'no_collector' ? '미전달' : '대기';

                        const execColor = log.execution_result === 'protocol_success' ? '#22c55e' : log.execution_result === 'protocol_failure' || log.execution_result === 'failure' ? '#ef4444' : log.execution_result === 'protocol_async' ? '#3b82f6' : log.execution_result === 'timeout' ? '#f97316' : '#9ca3af';
                        const execIcon = log.execution_result === 'protocol_success' ? 'fa-check-circle' : log.execution_result === 'protocol_failure' || log.execution_result === 'failure' ? 'fa-times-circle' : log.execution_result === 'protocol_async' ? 'fa-exchange-alt' : log.execution_result === 'timeout' ? 'fa-hourglass-end' : 'fa-clock';
                        const execText = log.execution_result === 'protocol_success' ? '성공' : log.execution_result === 'protocol_failure' || log.execution_result === 'failure' ? '실패' : log.execution_result === 'protocol_async' ? '비동기' : log.execution_result === 'timeout' ? '타임아웃' : '대기';

                        const verColor = log.verification_result === 'verified' ? '#22c55e' : log.verification_result === 'unverified' ? '#f59e0b' : log.verification_result === 'skipped' ? '#9ca3af' : '#d1d5db';
                        const verIcon = log.verification_result === 'verified' ? 'fa-check-double' : log.verification_result === 'unverified' ? 'fa-exclamation-triangle' : log.verification_result === 'skipped' ? 'fa-minus-circle' : 'fa-clock';
                        const verText = log.verification_result === 'verified' ? '반영됨' : log.verification_result === 'unverified' ? '미반영' : log.verification_result === 'skipped' ? '건너뜀' : '대기';

                        const finalBg = log.final_status === 'success' ? '#dcfce7' : log.final_status === 'failure' || log.final_status === 'timeout' ? '#fee2e2' : log.final_status === 'partial' ? '#fef3c7' : '#f3f4f6';
                        const finalColor = log.final_status === 'success' ? '#16a34a' : log.final_status === 'failure' || log.final_status === 'timeout' ? '#dc2626' : log.final_status === 'partial' ? '#d97706' : '#6b7280';
                        const finalText = log.final_status === 'success' ? '성공' : log.final_status === 'failure' ? '실패' : log.final_status === 'timeout' ? '타임아웃' : log.final_status === 'partial' ? '부분' : '대기';

                        return (
                          <tr key={log.id}>
                            <td className="font-bold text-primary">#{log.id}</td>
                            <td style={{ fontSize: '12px' }}>{log.username || '-'}</td>
                            <td>
                              <div className="font-semibold" style={{ fontSize: '13px' }}>{log.device_name || '-'}</div>
                              <div style={{ fontSize: '11px', color: '#6b7280' }}>{log.point_name || '-'}</div>
                              {log.protocol_type && <div style={{ fontSize: '10px', color: '#9ca3af' }}>{log.protocol_type}</div>}
                            </td>
                            <td style={{ textAlign: 'center', fontSize: '13px' }}>
                              <div style={{ color: '#6b7280', fontSize: '11px' }}>{log.old_value ?? '-'} →</div>
                              <div style={{ fontWeight: 600, color: '#1d4ed8' }}>{log.requested_value}</div>
                            </td>
                            <td style={{ textAlign: 'center' }}>
                              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '2px' }}>
                                <i className={`fas ${deliveryIcon}`} style={{ color: deliveryColor, fontSize: '14px' }} title={deliveryText}></i>
                                <span style={{ fontSize: '10px', color: deliveryColor }}>{deliveryText}</span>
                              </div>
                            </td>
                            <td style={{ textAlign: 'center' }}>
                              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '2px' }}>
                                <i className={`fas ${execIcon}`} style={{ color: execColor, fontSize: '14px' }} title={execText}></i>
                                <span style={{ fontSize: '10px', color: execColor }}>{execText}</span>
                                {log.duration_ms != null && <span style={{ fontSize: '9px', color: '#9ca3af' }}>{log.duration_ms}ms</span>}
                              </div>
                            </td>
                            <td style={{ textAlign: 'center' }}>
                              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '2px' }}>
                                <i className={`fas ${verIcon}`} style={{ color: verColor, fontSize: '14px' }} title={verText}></i>
                                <span style={{ fontSize: '10px', color: verColor }}>{verText}</span>
                              </div>
                            </td>
                            <td style={{ textAlign: 'center' }}>
                              <span style={{ display: 'inline-block', padding: '2px 8px', borderRadius: '10px', background: finalBg, color: finalColor, fontSize: '11px', fontWeight: 600 }}>
                                {finalText}
                              </span>
                            </td>
                            <td style={{ textAlign: 'center' }}>
                              {log.linked_alarm_id
                                ? <span style={{ fontSize: '11px', color: '#ef4444' }}><i className="fas fa-bell"></i> #{log.linked_alarm_id}</span>
                                : <span style={{ fontSize: '11px', color: '#d1d5db' }}>-</span>
                              }
                            </td>
                            <td style={{ fontSize: '12px', color: '#6b7280', whiteSpace: 'nowrap' }}>
                              {log.requested_at ? new Date(log.requested_at).toLocaleString('ko-KR') : '-'}
                            </td>
                          </tr>
                        );
                      })
                    )}
                  </tbody>
                </table>
              </div>
            )}
            {/* 제어이력 페이지네이션 */}
            <div className="mgmt-footer" style={{ padding: '12px 24px', borderTop: '1px solid var(--neutral-200)', background: 'white', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{ fontSize: '13px', color: '#6b7280' }}>총 {controlTotal}건</span>
              <div style={{ display: 'flex', gap: '6px' }}>
                <button className="mgmt-btn mgmt-btn-outline" disabled={controlPage <= 1} onClick={() => { const p = controlPage - 1; setControlPage(p); fetchControlLogs(p); }} style={{ padding: '4px 12px', fontSize: '12px' }}>이전</button>
                <span style={{ padding: '4px 12px', fontSize: '12px' }}>{controlPage}</span>
                <button className="mgmt-btn mgmt-btn-outline" disabled={controlPage * CONTROL_PAGE_SIZE >= controlTotal} onClick={() => { const p = controlPage + 1; setControlPage(p); fetchControlLogs(p); }} style={{ padding: '4px 12px', fontSize: '12px' }}>다음</button>
              </div>
            </div>
          </>
        )}
      </div>

      {/* 상세 보기 모달 */}
      {/* 상세 보기 모달 - Premium Layout */}
      {showDetailsModal && selectedEvent && (
        <AlarmHistoryDetailModal
          event={selectedEvent}
          onClose={() => setShowDetailsModal(false)}
          onAcknowledge={handleAcknowledge}
          onClear={handleClear}
        />
      )}
      {/* 알람 처리 모달 (확인/해제) */}
      <AlarmActionModal
        isOpen={showActionModal}
        type={actionType}
        alarmData={{
          rule_name: selectedEvent?.rule_name,
          alarm_message: selectedEvent?.alarm_message,
          severity: selectedEvent?.severity
        }}
        onConfirm={onConfirmAction}
        onCancel={() => setShowActionModal(false)}
        loading={actionLoading}
      />
    </ManagementLayout>
  );
};

export default AlarmHistory;