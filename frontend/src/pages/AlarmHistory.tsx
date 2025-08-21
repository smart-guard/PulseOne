// ============================================================================
// frontend/src/pages/AlarmHistory.tsx
// 알람 이력 페이지 - 2열 필터 패널 완성본
// ============================================================================

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { AlarmApiService, AlarmOccurrence, AlarmListParams, AlarmStatistics } from '../api/services/alarmApi';
import { Pagination } from '../components/common/Pagination';
import { usePagination } from '../hooks/usePagination';
import '../styles/base.css';
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
      start: new Date(Date.now() - 24 * 60 * 60 * 1000), // 24시간 전
      end: new Date()
    },
    severity: 'all',
    state: 'all',
    searchTerm: ''
  });
  
  // 페이징 훅 사용
  const pagination = usePagination({
    initialPage: 1,
    initialPageSize: 50,
    totalCount: 0
  });
  
  // 스크롤 위치 저장용
  const containerRef = useRef<HTMLDivElement>(null);
  const [scrollPosition, setScrollPosition] = useState(0);
  const [hasInitialLoad, setHasInitialLoad] = useState(false);

  // =============================================================================
  // API 호출 함수들 - AlarmApiService 사용
  // =============================================================================
  
  const fetchAlarmHistory = useCallback(async (isBackground = false) => {
    try {
      // 백그라운드 새로고침인 경우 스크롤 위치 저장
      if (isBackground && containerRef.current) {
        setScrollPosition(containerRef.current.scrollTop);
      }
      
      if (!isBackground) {
        setIsInitialLoading(true);
        setError(null);
      } else {
        setIsBackgroundRefreshing(true);
      }

      // API 파라미터 구성 - alarmApi.ts의 AlarmListParams 사용
      const params: AlarmListParams = {
        page: pagination.currentPage,
        limit: pagination.pageSize,
        ...(filters.severity !== 'all' && { severity: filters.severity }),
        ...(filters.state !== 'all' && { state: filters.state }),
        ...(filters.searchTerm && { search: filters.searchTerm }),
        date_from: filters.dateRange.start.toISOString(),
        date_to: filters.dateRange.end.toISOString()
      };

      console.log('알람 이력 조회 요청:', params);

      // AlarmApiService 사용
      const response = await AlarmApiService.getAlarmHistory(params);
      
      console.log('알람 이력 API 응답:', {
        success: response.success,
        dataType: typeof response.data,
        hasItems: response.data?.items ? true : false,
        itemsLength: response.data?.items?.length || 0
      });
      
      if (response.success && response.data) {
        const events = response.data.items || [];
        console.log('처리된 알람 이벤트:', events.length, events.slice(0, 2));
        setAlarmEvents(events);
        
        // 페이징 정보 업데이트
        if (response.data.pagination && typeof pagination.setTotalCount === 'function') {
          pagination.setTotalCount(response.data.pagination.total || 0);
        }
        
        // 첫 로딩 완료 표시
        if (!hasInitialLoad) {
          setHasInitialLoad(true);
        }
      } else {
        throw new Error(response.message || 'Failed to fetch alarm history');
      }
    } catch (err) {
      console.error('알람 이력 조회 실패:', err);
      setError(err instanceof Error ? err.message : '알람 이력을 불러오는데 실패했습니다');
      
      // 에러 시 빈 배열로 초기화
      if (!Array.isArray(alarmEvents)) {
        setAlarmEvents([]);
      }
    } finally {
      setIsInitialLoading(false);
      setIsBackgroundRefreshing(false);
      
      // 백그라운드 새로고침 후 스크롤 위치 복원
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
        // alarmApi.ts의 AlarmStatistics 인터페이스에 맞게 변환
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
      // 통계는 선택사항이므로 에러를 표시하지 않음
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
  // 유틸리티 함수들 - AlarmApiService 메서드 사용
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
  
  // 컴포넌트 마운트시 초기 데이터 로드
  useEffect(() => {
    fetchAlarmHistory();
    fetchStatistics();
  }, []);

  // 페이지 변경시 데이터 다시 로드
  useEffect(() => {
    if (hasInitialLoad) {
      fetchAlarmHistory(true);
    }
  }, [pagination.currentPage, pagination.pageSize]);

  // 필터 변경시 데이터 다시 로드
  useEffect(() => {
    if (hasInitialLoad) {
      fetchAlarmHistory();
    }
  }, [filters]);

  // 자동 새로고침 (30초마다)
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
    <div className="alarm-history-container" ref={containerRef}>
      {/* 페이지 헤더 */}
      <div className="page-header">
        <div className="header-content">
          <h1>
            <i className="fas fa-history"></i>
            알람 이력
          </h1>
          <p>시스템에서 발생한 모든 알람의 이력을 확인하고 분석할 수 있습니다.</p>
        </div>
        <div className="header-actions">
          <button
            className={`btn btn-secondary ${isBackgroundRefreshing ? 'refreshing' : ''}`}
            onClick={handleRefresh}
            disabled={isInitialLoading}
          >
            <i className={`fas fa-sync-alt ${isBackgroundRefreshing ? 'fa-spin' : ''}`}></i>
            새로고침
          </button>
          <button
            className="btn btn-primary"
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
        <div className="summary-panel">
          <div className="summary-card">
            <div className="summary-icon">
              <i className="fas fa-list-alt"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{statistics.occurrences.total_occurrences}</div>
              <div className="summary-label">총 이벤트</div>
            </div>
          </div>

          <div className="summary-card critical">
            <div className="summary-icon">
              <i className="fas fa-exclamation-triangle"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{statistics.occurrences.active_alarms}</div>
              <div className="summary-label">활성 알람</div>
            </div>
          </div>

          <div className="summary-card warning">
            <div className="summary-icon">
              <i className="fas fa-check-circle"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{statistics.occurrences.acknowledged_alarms}</div>
              <div className="summary-label">확인된 알람</div>
            </div>
          </div>

          <div className="summary-card success">
            <div className="summary-icon">
              <i className="fas fa-times-circle"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{statistics.occurrences.cleared_alarms}</div>
              <div className="summary-label">해제된 알람</div>
            </div>
          </div>

          <div className="summary-card">
            <div className="summary-icon">
              <i className="fas fa-chart-bar"></i>
            </div>
            <div className="summary-content">
              <div className="summary-value">{statistics.rules.total_rules}</div>
              <div className="summary-label">총 규칙</div>
            </div>
          </div>
        </div>
      )}

      {/* 에러 메시지 */}
      {error && (
        <div className="error-alert">
          <i className="fas fa-exclamation-circle"></i>
          <span>{error}</span>
          <button onClick={() => setError(null)}>
            <i className="fas fa-times"></i>
          </button>
        </div>
      )}

      {/* 필터 패널 - 진짜 한줄 구조 */}
      <div className="filter-panel">
        {/* 기간 필터 */}
        <div className="filter-group">
          <label>기간</label>
          <div className="date-range">
            <input
              type="datetime-local"
              value={filters.dateRange.start.toISOString().slice(0, 16)}
              onChange={(e) => handleFilterChange({
                dateRange: { ...filters.dateRange, start: new Date(e.target.value) }
              })}
            />
            <span>~</span>
            <input
              type="datetime-local"
              value={filters.dateRange.end.toISOString().slice(0, 16)}
              onChange={(e) => handleFilterChange({
                dateRange: { ...filters.dateRange, end: new Date(e.target.value) }
              })}
            />
          </div>
        </div>

        {/* 심각도 필터 */}
        <div className="filter-group">
          <label>심각도</label>
          <select
            value={filters.severity}
            onChange={(e) => handleFilterChange({ severity: e.target.value })}
          >
            <option value="all">전체</option>
            <option value="critical">Critical</option>
            <option value="high">High</option>
            <option value="medium">Medium</option>
            <option value="low">Low</option>
            <option value="info">Info</option>
          </select>
        </div>

        {/* 상태 필터 */}
        <div className="filter-group">
          <label>상태</label>
          <select
            value={filters.state}
            onChange={(e) => handleFilterChange({ state: e.target.value })}
          >
            <option value="all">전체</option>
            <option value="active">활성</option>
            <option value="acknowledged">확인됨</option>
            <option value="cleared">해제됨</option>
          </select>
        </div>

        {/* 검색창 */}
        <div className="filter-group">
          <label>검색</label>
          <div className="search-group">
            <input
              type="text"
              placeholder="알람명, 디바이스명으로 검색..."
              value={filters.searchTerm}
              onChange={(e) => handleFilterChange({ searchTerm: e.target.value })}
              onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
            />
            <button onClick={handleSearch}>
              <i className="fas fa-search"></i>
            </button>
          </div>
        </div>
      </div>

      {/* 결과 정보 - 뷰 컨트롤 포함 */}
      <div className="result-info">
        <div className="result-info-left">
          <div className="result-count">
            총 {pagination.totalCount}개의 알람 이력
            {Array.isArray(alarmEvents) && (
              <span className="current-showing"> (현재 {alarmEvents.length}개 표시)</span>
            )}
          </div>
        </div>
        
        <div className="result-info-right">
          <div className="date-range-display">
            {filters.dateRange.start.toLocaleDateString('ko-KR')} ~ {filters.dateRange.end.toLocaleDateString('ko-KR')}
          </div>
          
          <div className="view-controls">
            <button
              className={`view-btn ${viewMode === 'list' ? 'active' : ''}`}
              onClick={() => setViewMode('list')}
            >
              <i className="fas fa-list"></i>
              목록
            </button>
            <button
              className={`view-btn ${viewMode === 'timeline' ? 'active' : ''}`}
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
        <div className="loading-state">
          <div className="loading-spinner"></div>
          <p>알람 이력을 불러오는 중...</p>
        </div>
      ) : !Array.isArray(alarmEvents) || alarmEvents.length === 0 ? (
        <div className="empty-state">
          <div className="empty-icon">
            <i className="fas fa-history"></i>
          </div>
          <h3>알람 이력이 없습니다</h3>
          <p>
            {!Array.isArray(alarmEvents) 
              ? "데이터 형식 오류가 발생했습니다. 새로고침을 시도해보세요."
              : "검색 조건에 맞는 알람 이력을 찾을 수 없습니다. 필터를 조정해보세요."
            }
          </p>
        </div>
      ) : viewMode === 'list' ? (
        /* 목록 뷰 */
        <div className="history-table-container">
          <table className="history-table">
            <thead>
              <tr>
                <th>ID</th>
                <th>심각도</th>
                <th>디바이스/포인트</th>
                <th>메시지</th>
                <th>상태</th>
                <th>발생시간</th>
                <th>지속시간</th>
                <th>액션</th>
              </tr>
            </thead>
            <tbody>
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
                  <tr
                    key={eventId}
                    className={`history-row ${severity}`}
                    style={{ 
                      animationDelay: `${index * 0.05}s`,
                      opacity: isBackgroundRefreshing ? 0.7 : 1 
                    }}
                  >
                    <td>
                      <span className="event-id">#{eventId}</span>
                    </td>
                    
                    <td>
                      <div className="priority-cell">
                        <div 
                          className="priority-indicator"
                          style={{ backgroundColor: getPriorityColor(severity) }}
                        ></div>
                        <span>{severity.toUpperCase()}</span>
                      </div>
                    </td>
                    
                    <td>
                      <div className="source-cell">
                        <div className="source-name">{deviceName}</div>
                        <div className="source-location">{dataPointName}</div>
                      </div>
                    </td>
                    
                    <td>
                      <div className="message-cell">
                        <div className="message-text">{message}</div>
                        {triggeredValue !== null && triggeredValue !== undefined && (
                          <div className="triggered-value">값: {triggeredValue}</div>
                        )}
                      </div>
                    </td>
                    
                    <td>
                      <div className="status-cell">
                        <div 
                          className="status-indicator"
                          style={{ color: getStatusColor(state) }}
                        >
                          <i className={getStatusIcon(state)}></i>
                        </div>
                        <span>{getStatusText(state)}</span>
                      </div>
                    </td>
                    
                    <td>
                      <div className="time-cell">
                        <div className="time-main">
                          {new Date(occurrenceTime).toLocaleDateString('ko-KR')}
                        </div>
                        <div className="time-detail">
                          {new Date(occurrenceTime).toLocaleTimeString('ko-KR')}
                        </div>
                      </div>
                    </td>
                    
                    <td>
                      <div className="duration-cell">
                        {formatDuration(occurrenceTime, event?.cleared_time || event?.acknowledged_time)}
                      </div>
                    </td>
                    
                    <td>
                      <div className="action-cell">
                        <button
                          className="btn btn-small btn-secondary"
                          onClick={() => handleViewDetails(event)}
                        >
                          <i className="fas fa-eye"></i>
                          상세
                        </button>
                      </div>
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      ) : (
        /* 타임라인 뷰 */
        <div className="timeline-view">
          <div className="timeline-container">
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
                <div key={eventId} className={`timeline-item ${severity}`}>
                  <div className="timeline-marker">
                    <div 
                      className="timeline-dot"
                      style={{ backgroundColor: getPriorityColor(severity) }}
                    ></div>
                  </div>
                  <div className="timeline-content">
                    <div className="timeline-header">
                      <div className="timeline-title">
                        <span 
                          className="priority-badge"
                          style={{ backgroundColor: getPriorityColor(severity) }}
                        >
                          {severity.toUpperCase()}
                        </span>
                        <span>{ruleName}</span>
                      </div>
                      <div className="timeline-time">
                        {new Date(occurrenceTime).toLocaleString('ko-KR')}
                      </div>
                    </div>
                    <div className="timeline-body">
                      <div className="timeline-message">{message}</div>
                      <div className="timeline-source">
                        {deviceName} • {dataPointName}
                      </div>
                      <div className="timeline-footer">
                        <div className="timeline-status">
                          <i 
                            className={getStatusIcon(state)} 
                            style={{ color: getStatusColor(state) }}
                          ></i>
                          {getStatusText(state)}
                        </div>
                        <div className="timeline-duration">
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
        <div className="modal-overlay">
          <div className="modal-container">
            <div className="modal-header">
              <h3>알람 상세 정보</h3>
              <button onClick={() => setShowDetailsModal(false)}>
                <i className="fas fa-times"></i>
              </button>
            </div>
            <div className="modal-content">
              <div className="detail-section">
                <h4>기본 정보</h4>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>이벤트 ID</label>
                    <span>#{selectedEvent.id}</span>
                  </div>
                  <div className="detail-item">
                    <label>알람 규칙</label>
                    <span>{selectedEvent.rule_name}</span>
                  </div>
                  <div className="detail-item">
                    <label>심각도</label>
                    <span 
                      className="priority-badge"
                      style={{ backgroundColor: getPriorityColor(selectedEvent.severity) }}
                    >
                      {selectedEvent.severity.toUpperCase()}
                    </span>
                  </div>
                  <div className="detail-item">
                    <label>상태</label>
                    <span 
                      className="status-badge"
                      style={{ color: getStatusColor(selectedEvent.state) }}
                    >
                      <i className={getStatusIcon(selectedEvent.state)}></i>
                      {getStatusText(selectedEvent.state)}
                    </span>
                  </div>
                </div>
              </div>

              <div className="detail-section">
                <h4>시간 정보</h4>
                <div className="detail-grid">
                  <div className="detail-item">
                    <label>발생시간</label>
                    <span>{new Date(selectedEvent.occurrence_time).toLocaleString('ko-KR')}</span>
                  </div>
                  {selectedEvent.acknowledged_time && (
                    <div className="detail-item">
                      <label>확인시간</label>
                      <span>{new Date(selectedEvent.acknowledged_time).toLocaleString('ko-KR')}</span>
                    </div>
                  )}
                  {selectedEvent.cleared_time && (
                    <div className="detail-item">
                      <label>해제시간</label>
                      <span>{new Date(selectedEvent.cleared_time).toLocaleString('ko-KR')}</span>
                    </div>
                  )}
                  <div className="detail-item">
                    <label>지속시간</label>
                    <span>{formatDuration(selectedEvent.occurrence_time, selectedEvent.cleared_time || selectedEvent.acknowledged_time)}</span>
                  </div>
                </div>
              </div>

              <div className="detail-section">
                <h4>메시지 및 설명</h4>
                <div className="message-content">
                  <p><strong>메시지:</strong> {selectedEvent.alarm_message}</p>
                  {selectedEvent.trigger_value !== null && selectedEvent.trigger_value !== undefined && (
                    <p><strong>발생값:</strong> {selectedEvent.trigger_value}</p>
                  )}
                </div>
              </div>

              {(selectedEvent.acknowledged_by || selectedEvent.acknowledge_comment) && (
                <div className="detail-section">
                  <h4>확인 정보</h4>
                  <div className="detail-grid">
                    {selectedEvent.acknowledged_by && (
                      <div className="detail-item">
                        <label>확인자</label>
                        <span>{selectedEvent.acknowledged_by}</span>
                      </div>
                    )}
                    {selectedEvent.acknowledge_comment && (
                      <div className="detail-item full-width">
                        <label>확인 메모</label>
                        <span>{selectedEvent.acknowledge_comment}</span>
                      </div>
                    )}
                  </div>
                </div>
              )}

              {selectedEvent.clear_comment && (
                <div className="detail-section">
                  <h4>해제 정보</h4>
                  <div className="detail-grid">
                    <div className="detail-item full-width">
                      <label>해제 메모</label>
                      <span>{selectedEvent.clear_comment}</span>
                    </div>
                  </div>
                </div>
              )}
            </div>
            <div className="modal-footer">
              <button 
                className="btn btn-secondary"
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